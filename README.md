# VSFSck — A Consistency Checker for Very Simple File System (VSFS)

`vsfsck` is a file system consistency checker written in C for a custom, simplified virtual file system called **VSFS**. It inspects a raw VSFS disk image, reports structural inconsistencies, and can optionally repair a subset of them.

Built for **CSE321 (Operating Systems) — Project 2** at BRAC University.

## What it checks

VSFS is a 64-block (4096 bytes/block) file system image with the following layout:

| Block(s) | Purpose |
|---|---|
| 0 | Superblock |
| 1 | Inode bitmap |
| 2 | Data bitmap |
| 3–7 | Inode table (5 blocks, 256-byte inodes) |
| 8–63 | Data blocks |

`vsfsck` validates:

1. **Superblock Validator** — magic number (`0xD34D`), block size, total block count, bitmap/inode-table/data-block pointer ranges, inode size, and inode count (auto-corrects inode count if it doesn't match the computed value).
2. **Inode Bitmap Consistency Checker** — every inode with `links_count > 0` and `deletion_time == 0` is marked used in the inode bitmap.
3. **Data Bitmap Consistency Checker** — every data block referenced by a valid inode is marked used in the data bitmap.
4. **Duplicate Checker** — detects data blocks referenced by more than one inode.
5. **Bad Block / Orphan Checker** — detects blocks marked used in the bitmap but not actually referenced by any inode, and unmarks them when fixing.

## Build

```bash
make
```

This produces the `vsfsck` binary. Requires `gcc` and a POSIX-ish environment (Linux/macOS/WSL).

## Usage

```bash
./vsfsck <image_path> <fix (0 or 1)>
```

- `<image_path>` — path to the VSFS image file (e.g. `vsfs.img`)
- `<fix>` — `0` to only report inconsistencies, `1` to also fix them in place and re-verify

Examples:

```bash
# Report-only, no changes to the image
./vsfsck vsfs.img 0

# Report and repair, then re-check the image
./vsfsck vsfs.img 1
```

> ⚠️ Fix mode modifies the image file directly. Keep a backup of the original `vsfs.img` if you want to compare before/after.

## Repo contents

- `vsfsck.c` — the checker/fixer implementation
- `vsfs.img` — a sample (intentionally corrupted) VSFS image for testing
- `Makefile` — build/run/clean targets

## Notes / known limitations

- The current implementation checks only the **first direct block pointer** per inode (`direct_block`); it does not yet walk multiple direct pointers or indirect/double-indirect/triple-indirect blocks. Extending block traversal to cover those would make the duplicate and bad-block checks fully complete per the spec.
- Bad-block detection (blocks referenced with an index outside the valid data range) is partially covered via the bitmap/reference checks; a dedicated explicit range check on raw pointer values could be added for full coverage of that feature.

## Course context

CSE321 Project 2, group assignment (max 3 members). See the original assignment PDF for full specification and grading breakdown.
