# my_cp
Is more or less functional implementation of binutils/cp. Will be developed futher
# features:
1) file to file copy
2) file to dir copy
3) no overwriting
4) insufficient rights and other errors informing using perror
6) linux rights 'rwx' for user, group, others saving on copy
7) uses mmap() mapping files to address space for copyng, also includes commented out function for block file--/bs_size/-->ram--/bs_size/-->file copy
8) Checking if source and destination are neither regular files nor directories (tested on /dev/urandom)

# known issues:
1) while attempting to copy to unexisting directory, error message is not that correct:"Это каталог".

