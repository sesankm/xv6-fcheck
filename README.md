# fcheck for xv6
## Reads file system and determines consistency of the following
1. Each inode is either unallocated or one of the valid types (T_FILE, T_DIR, T_DEV).
If not, print ERROR: bad inode.
2. For in-use inodes, each block address that is used by the inode is valid (points to
a valid data block address within the image). If the direct block is used and is 
invalid, print ERROR: bad direct address in inode.; if the indirect block is in
use and is invalid, print ERROR: bad indirect address in inode.
3. Root directory exists, its inode number is 1, and the parent of the root directory is
itself. If not, print ERROR: root directory does not exist.
4. Each directory contains . and .. entries, and the . entry points to the directory
itself. If not, print ERROR: directory not properly formatted.
5. For in-use inodes, each block address in use is also marked in use in the bitmap.
If not, print ERROR: address used by inode but marked free in bitmap.
6. For blocks marked in-use in bitmap, the block should actually be in-use in an
inode or indirect block somewhere. If not, print ERROR: bitmap marks block
in use but it is not in use.
7. For in-use inodes, each direct address in use is only used once. If not,
print ERROR: direct address used more than once.
8. For in-use inodes, each indirect address in use is only used once. If not,
print ERROR: indirect address used more than once.
9. For all inodes marked in use, each must be referred to in at least one directory. If
not, print ERROR: inode marked use but not found in a directory.
10. For each inode number that is referred to in a valid directory, it is actually marked
in use. If not, print ERROR: inode referred to in directory but marked
free.
11. Reference counts (number of links) for regular files match the number of times
file is referred to in directories (i.e., hard links work correctly). If not, print ERROR:
bad reference count for file.
12. No extra links allowed for directories (each directory only appears in one other
directory). If not, print ERROR: directory appears more than once in file
system.

## Usage
`fcheck path_to_image_file`

## Compiling
`gcc fcheck.c -o fcheck -Wall -Werror -O`

### examples of fs images to test in "tests" directory

---

xv6 is a re-implementation of Dennis Ritchie's and Ken Thompson's Unix
Version 6 (v6).  xv6 loosely follows the structure and style of v6,
but is implemented for a modern x86-based multiprocessor using ANSI C.

ACKNOWLEDGMENTS

xv6 is inspired by John Lions's Commentary on UNIX 6th Edition (Peer
to Peer Communications; ISBN: 1-57398-013-7; 1st edition (June 14,
2000)). See also http://pdos.csail.mit.edu/6.828/2007/v6.html, which
provides pointers to on-line resources for v6.

xv6 borrows code from the following sources:
    JOS (asm.h, elf.h, mmu.h, bootasm.S, ide.c, console.c, and others)
    Plan 9 (bootother.S, mp.h, mp.c, lapic.c)
    FreeBSD (ioapic.c)
    NetBSD (console.c)

The following people made contributions:
    Russ Cox (context switching, locking)
    Cliff Frey (MP)
    Xiao Yu (MP)
    Nickolai Zeldovich
    Austin Clements

In addition, we are grateful for the patches contributed by Greg
Price, Yandong Mao, and Hitoshi Mitake.

The code in the files that constitute xv6 is
Copyright 2006-2007 Frans Kaashoek, Robert Morris, and Russ Cox.

ERROR REPORTS

If you spot errors or have suggestions for improvement, please send
email to Frans Kaashoek and Robert Morris (kaashoek,rtm@csail.mit.edu). 

BUILDING AND RUNNING XV6

To build xv6 on an x86 ELF machine (like Linux or FreeBSD), run "make".
On non-x86 or non-ELF machines (like OS X, even on x86), you will
need to install a cross-compiler gcc suite capable of producing x86 ELF
binaries.  See http://pdos.csail.mit.edu/6.828/2007/tools.html.
Then run "make TOOLPREFIX=i386-jos-elf-".

To run xv6, you can use Bochs or QEMU, both PC simulators.
Bochs makes debugging easier, but QEMU is much faster. 
To run in Bochs, run "make bochs" and then type "c" at the bochs prompt.
To run in QEMU, run "make qemu".

