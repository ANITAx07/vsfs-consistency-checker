#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 64
#define INODE_SIZE 256

// Superblock structure
typedef struct {
    uint16_t magic_bytes;         // Should be 0xD34D to validate the superblock
    uint32_t block_size;          // Block size in bytes, should be 4096
    uint32_t total_blocks;        // Total number of blocks in the file system (64)
    uint32_t inode_bitmap_block;  // The block number where the inode bitmap is located
    uint32_t data_bitmap_block;   // The block number where the data bitmap is located
    uint32_t inode_table_start;   // The starting block of the inode table
    uint32_t first_data_block;    // The first block used for actual data storage
    uint32_t inode_size;          // Size of each inode, should be 256 bytes
    uint32_t inode_count;         // Total number of inodes
    uint8_t reserved[4058];       // Reserved space for future use (4058 bytes)
} Superblock;

// Inode structure
typedef struct {
    uint32_t mode;                // File type and permissions
    uint32_t user_id;             // Owner's user ID
    uint32_t group_id;            // Owner's group ID
    uint32_t file_size;           // File size in bytes
    uint32_t last_access_time;    // Last access time (timestamp)
    uint32_t creation_time;       // Creation time (timestamp)
    uint32_t last_mod_time;       // Last modification time (timestamp)
    uint32_t deletion_time;       // Time of deletion (if the file is deleted)
    uint32_t links_count;         // Number of hard links to the file
    uint32_t data_block_count;    // Number of data blocks allocated to this file
    uint32_t direct_block;        // Pointer to the direct data block
    uint32_t indirect_block;      // Pointer to the single indirect data block
    uint32_t double_indirect_block; // Pointer to the double indirect data block
    uint32_t triple_indirect_block; // Pointer to the triple indirect data block
    uint8_t reserved[156];        // Reserved bytes for future use
} Inode;

// Globals for tracking block usage
int referenced_blocks[TOTAL_BLOCKS] = {0}; // Track which blocks are being used
int block_references[TOTAL_BLOCKS] = {0};  // Track how many times a block is referenced

// Function prototypes
void read_superblock(FILE *fs_img, Superblock *sb);
void read_inode(FILE *fs_img, Inode *inode, int inode_num, uint32_t inode_table_start);
void verify_and_fix_data_blocks(FILE *fs_img, Superblock *sb, int do_fix);
void verify_and_fix_inode_bitmap(FILE *fs_img, Superblock *sb, int do_fix);
void verify_and_fix_data_bitmap(FILE *fs_img, Superblock *sb, int do_fix);
void verify_and_fix_superblock(FILE *fs_img, Superblock *sb);
void print_usage();

int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage();
        return 1;
    }

    FILE *fs_img = fopen(argv[1], "r+b");
    if (fs_img == NULL) {
        perror("Failed to open file");
        return 1;
    }

    Superblock sb;
    read_superblock(fs_img, &sb);

    int do_fix = atoi(argv[2]);  // Should we actually fix the issues? (1 = fix, 0 = just report)

    // Perform checks and fix issues if needed
    verify_and_fix_superblock(fs_img, &sb);
    verify_and_fix_inode_bitmap(fs_img, &sb, do_fix);
    verify_and_fix_data_bitmap(fs_img, &sb, do_fix);
    verify_and_fix_data_blocks(fs_img, &sb, do_fix);

    // Recheck after applying fixes
    if (do_fix) {
        printf("\n--- Rechecking after fixes ---\n");
        memset(referenced_blocks, 0, sizeof(referenced_blocks)); // Reset block references
        memset(block_references, 0, sizeof(block_references)); // Reset block usage
        rewind(fs_img);
        read_superblock(fs_img, &sb); // Re-read the superblock after fixes

        // Re-run the checks after applying fixes
        verify_and_fix_superblock(fs_img, &sb);
        verify_and_fix_inode_bitmap(fs_img, &sb, 0); // Don't fix this time, just recheck
        verify_and_fix_data_bitmap(fs_img, &sb, 0); // Recheck only
        verify_and_fix_data_blocks(fs_img, &sb, 0); // Recheck only
    }

    printf("VSFS check completed.\n");
    fclose(fs_img); // Done with the file system image
    return 0;
}

void print_usage() {
    // Let's make this more friendly and human-like
    printf("Usage: vsfsck <vsfs.img> <fix (0 or 1)>\n");
    printf("  <vsfs.img>      : Path to the file system image (you'll want to check this one)\n");
    printf("  <fix (0 or 1)>  : 1 to fix errors, 0 to just report (if you want to play safe)\n");
}

// Read the superblock from the file system image
void read_superblock(FILE *fs_img, Superblock *sb) {
    fseek(fs_img, 0, SEEK_SET);
    fread(sb, sizeof(Superblock), 1, fs_img); // Grab the whole superblock
}

// Read an inode from the inode table
void read_inode(FILE *fs_img, Inode *inode, int inode_num, uint32_t inode_table_start) {
    fseek(fs_img, inode_table_start * BLOCK_SIZE + inode_num * INODE_SIZE, SEEK_SET); // Position the file pointer
    fread(inode, sizeof(Inode), 1, fs_img); // Read the inode data
}

// Superblock verification
void verify_and_fix_superblock(FILE *fs_img, Superblock *sb) {
    if (sb->magic_bytes != 0xD34D)
        printf("Oops! The superblock magic bytes don't match. Something's off.\n");
    if (sb->block_size != BLOCK_SIZE)
        printf("Uh-oh, block size mismatch! Expected %d, but found %d.\n", BLOCK_SIZE, sb->block_size);
    if (sb->total_blocks != TOTAL_BLOCKS)
        printf("Total block mismatch. Expected 64, but found %d.\n", sb->total_blocks);
    if (sb->inode_bitmap_block < 1 || sb->inode_bitmap_block >= TOTAL_BLOCKS)
        printf("The inode bitmap block is invalid, need to check that out!\n");
    if (sb->data_bitmap_block < 1 || sb->data_bitmap_block >= TOTAL_BLOCKS)
        printf("Hmm, the data bitmap block seems off. Let's verify that.\n");
    if (sb->inode_table_start < 3 || sb->inode_table_start >= TOTAL_BLOCKS)
        printf("The inode table start block seems out of range. Time to investigate.\n");
    if (sb->first_data_block < 8 || sb->first_data_block >= TOTAL_BLOCKS)
        printf("First data block is a bit fishy. It should be between 8 and 63.\n");
    if (sb->inode_size != INODE_SIZE)
        printf("Warning: Inode size doesn't match the expected 256 bytes.\n");

    // Dynamic inode count check
    uint32_t inode_count = (sb->inode_table_start - 3) * BLOCK_SIZE / INODE_SIZE;
    if (sb->inode_count != inode_count) {
        printf("Whoa! Inode count mismatch. Expected: %u, but got: %u. Let's fix this.\n", inode_count, sb->inode_count);
        // Fix the inode count in the superblock
        sb->inode_count = inode_count; // Update the superblock with the correct inode count
        // Write the updated superblock back to the image
        fseek(fs_img, 0, SEEK_SET);
        fwrite(sb, sizeof(Superblock), 1, fs_img);
    }
}

// Inode bitmap verification and fix
void verify_and_fix_inode_bitmap(FILE *fs_img, Superblock *sb, int do_fix) {
    uint8_t inode_bitmap[BLOCK_SIZE] = {0};
    fseek(fs_img, sb->inode_bitmap_block * BLOCK_SIZE, SEEK_SET);
    fread(inode_bitmap, BLOCK_SIZE, 1, fs_img);

    for (int i = 0; i < sb->inode_count; i++) {
        Inode inode;
        read_inode(fs_img, &inode, i, sb->inode_table_start);
        if (inode.links_count == 0 || inode.deletion_time != 0) continue;

        if (!(inode_bitmap[i / 8] & (1 << (i % 8)))) {
            printf("Fixing: Inode %d is in use but not marked in the bitmap. Let's fix that!\n", i);
            if (do_fix) inode_bitmap[i / 8] |= (1 << (i % 8));
        }
    }

    if (do_fix) {
        fseek(fs_img, sb->inode_bitmap_block * BLOCK_SIZE, SEEK_SET);
        fwrite(inode_bitmap, BLOCK_SIZE, 1, fs_img);
    }
}

// Data bitmap verification and fix
void verify_and_fix_data_bitmap(FILE *fs_img, Superblock *sb, int do_fix) {
    uint8_t data_bitmap[BLOCK_SIZE] = {0};
    fseek(fs_img, sb->data_bitmap_block * BLOCK_SIZE, SEEK_SET);
    fread(data_bitmap, BLOCK_SIZE, 1, fs_img);

    for (int i = 0; i < sb->inode_count; i++) {
        Inode inode;
        read_inode(fs_img, &inode, i, sb->inode_table_start);
        if (inode.links_count == 0 || inode.deletion_time != 0) continue;

        uint32_t blk = inode.direct_block;
        if (blk >= 8 && blk < TOTAL_BLOCKS) {
            referenced_blocks[blk]++;
            block_references[blk]++;
            if (!(data_bitmap[blk / 8] & (1 << (blk % 8)))) {
                printf("Fixing: Block %u used by inode %d not marked in the bitmap. Marking it now!\n", blk, i);
                if (do_fix) data_bitmap[blk / 8] |= (1 << (blk % 8));
            }
        }
    }

    if (do_fix) {
        fseek(fs_img, sb->data_bitmap_block * BLOCK_SIZE, SEEK_SET);
        fwrite(data_bitmap, BLOCK_SIZE, 1, fs_img);
    }
}

// Data block reference check and duplicate detection
void verify_and_fix_data_blocks(FILE *fs_img, Superblock *sb, int do_fix) {
    uint8_t data_bitmap[BLOCK_SIZE] = {0};
    fseek(fs_img, sb->data_bitmap_block * BLOCK_SIZE, SEEK_SET);
    fread(data_bitmap, BLOCK_SIZE, 1, fs_img);

    for (int i = 8; i < TOTAL_BLOCKS; i++) {
        if ((data_bitmap[i / 8] & (1 << (i % 8))) && referenced_blocks[i] == 0) {
            printf("Fixing: Block %d is marked used but not referenced. Let's unmark it.\n", i);
            if (do_fix) data_bitmap[i / 8] &= ~(1 << (i % 8));
        }
        if (block_references[i] > 1) {
            printf("Oh no! Block %d is referenced multiple times. This needs to be fixed!\n", i);
        }
    }

    if (do_fix) {
        fseek(fs_img, sb->data_bitmap_block * BLOCK_SIZE, SEEK_SET);
        fwrite(data_bitmap, BLOCK_SIZE, 1, fs_img);
    }
}

