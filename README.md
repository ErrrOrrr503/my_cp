# my_cp
Is more or less functional implementation of binutils/cp. Will be developed futher
# features:
1) file to file copy
2) file to dir copy
3) dir to dir recursive copying (getdents64 used) finally...
4) symlink copying. Note: target is preserved, not changed to point to same file
5) fifo copying
6) block and char file copying
7) default: no overwriting
8) -f (force) option support for overwriting
9) -u option to preserve uid and git, see issue 4 for more info
10) -t option to preserve last access and modify times in nanoseconds
11) options can be differently combinated: <-t -u> <-tu -f> <-ftu>
12) insufficient rights and other errors informing using perror
13) linux rights 'rwx' for user, group, others saving on copy
14) block read-write file copy
15) Checking if source and destination are neither regular files nor directories (tested on /dev/urandom, /dev/null)

# known issues:
1) while attempting to copy to unexisting directory, error message is not that correct:"Это каталог". Well, in qute many cases messages of perror can't be correct.
2) After switching to openat from manual name generation can't copy to wronly directories, due to open () can't open dir with O_WRONLY.
3) while -f copying regfile->symlink, the file is copyed to the destination of the link, if the link can't be resolved, it is replaced by file. It is not that obvious.
4) When non -u copying and destination exists, uid and gid of destination will remain same and won't be changed to uid and gid of the process.
5) May be qute elaborate undefined behaviour like endless chains while copying dir to dir: ("my_cp ./dir ./dir/dir1) with SEGFAULTS and so on. And dir symlinks are not tested... disasterous...
