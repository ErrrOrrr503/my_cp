# my_cp
Is more or less functional implementation of binutils/cp. Will be developed futher
# features:
file to file copy
file to dir copy
dir to dir recursive copying (getdents64 used) finally...
symlink copying. Note: target is preserved, not changed to point to same file
default: no overwriting
-f (force) option support for overwriting
-u option to preserve uid and git, see issue 4 for more info
-t option to preserve last access and modify times in nanoseconds
options can be differently combinated: <-t -u> <-tu -f> <-ftu>
insufficient rights and other errors informing using perror
linux rights 'rwx' for user, group, others saving on copy
block read-write file copy
Checking if source and destination are neither regular files nor directories (tested on /dev/urandom, /dev/null)

# known issues:
while attempting to copy to unexisting directory, error message is not that correct:"Это каталог". Well, in qute many cases messages of perror can't be correct.
After switching to openat from manual name generation can't copy to wronly directories, due to open () can't open dir with O_WRONLY.
while -f copying regfile->symlink, the file is copyed to the destination of the link, if the link can't be resolved, it is replaced by file. It is not that obvious.
When non -u copying and destination exists, uid and gid of destination will remain same and won't be changed to uid and gid of the process.
May be qute elaborate undefined behaviour like endless chains while copying dir to dir: ("my_cp ./dir ./dir/dir1) with SEGFAULTS and so on. And dir symlinks are not tested... disasterous...
