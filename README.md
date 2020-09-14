# my_cp
Is more or less functional implementation of binutils/cp. Will be developed futher
# features:
1) file to file copy
2) file to dir copy
3) no overwriting
4) insufficient rights informing
5) no free space informing (not tested yet)
6) linux rights 'rwx' for user, group, others saving on copy
7) uses mmap() mapping files to address space for copyng, also includes commented out function for block file--/bs_size/-->ram--/bs_size/-->file copy
