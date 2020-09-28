# my_cp
Is more or less functional implementation of binutils/cp. Will be developed futher
# features:
1) file to file copy
2) file to dir copy
3) symlink copying. Note: target is preserved, not changed to point to same file
4) default: no overwriting
5) -f (force) option support for overwriting
6) -u option to preserve uid and git, see issue 4 for more info
7) -t option to preserve last access and modify times in nanoseconds
8) options can be differently combinated: <-t -u> <-tu -f> <-ftu>
9) insufficient rights and other errors informing using perror
10) linux rights 'rwx' for user, group, others saving on copy
11) block read-write file copy
12) Checking if source and destination are neither regular files nor directories (tested on /dev/urandom, /dev/null)

# known issues:
1) while attempting to copy to unexisting directory, error message is not that correct:"Это каталог". Well, in qute many cases messages of perror can't be correct.
2) After switching to openat from manual name generation can't copy to wronly directories, due to open () can't open dir with O_WRONLY.
3) while -f copying regfile->symlink, the file is copyed to the destination of the link, if the link can't be resolved, it is replaced by file. It is not that obvious.
4) When non -u copying and destination exists, uid and gid of destination will remain same and won't be changed to uid and gid of the process.
