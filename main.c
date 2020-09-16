#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

extern int errno;

//negative error codes for function

#define _ERR_IO_READ 10
#define _ERR_IO_WRITE 11
#define _ERR_MAPPING_SOURCE 12
#define _ERR_MAPPING_DEST 13
#define _ERR_NO_FREE_SPACE 20

#define _ERR_ARG_COUNT 1
#define _ERR_ARG_TYPE 2

#define _ERR_NO_PERMIT 3
#define _ERR_NO_FILE 4
#define _ERR_FILE_EXISTS 5

#define _ERR_FIXME 666 //typical return on lack of implementation

const mode_t no_mode = -1;

typedef int fdesc;

enum filetype {
	nofile,
	dir,
	regfile,
	nosupport
};

struct fileinfo {
	char *name;
	struct stat st;
	fdesc fd;
	enum filetype type;
};

int fcopy_mmap (struct fileinfo *source_f, struct fileinfo *dest_f);
int open_fileinfo (struct fileinfo *file_f, int flags, mode_t mode);
int stat_fileinfo (struct fileinfo *file_f);
int file_to_file_copy (struct fileinfo *source_f, struct fileinfo *dest_f);
int file_to_dir_copy (struct fileinfo *source_f, struct fileinfo *dest_f);


int main (int argc, char *argv[])
{
	//arg checking
	if (argc != 3) {
		printf ("Usage: %s <source> <destenition>\n", argv[0]);
		return _ERR_ARG_COUNT;
	}
	int ret = 0; //common var for return value

	//sorce open
	struct fileinfo source_f = {0};
	source_f.name = argv[1];
	ret = open_fileinfo (&source_f, O_RDONLY, no_mode);
	if (ret)
		return ret;
	
	//dest stat
	struct fileinfo dest_f = {0};
	dest_f.name = argv[2];
	stat_fileinfo (&dest_f);

	//non-reg file processing
	if (source_f.type != regfile) {
		//recursive copying will be somewhat here
		printf ("source is not a regular file\n");
		close (source_f.fd);
		return _ERR_FIXME;
	}
	if (dest_f.type == nosupport) {
		printf ("destination is neither regular file nor directory\n");
		close (source_f.fd);
		return _ERR_ARG_TYPE;
	}
	//here source is reg file and dest is reg file of dir
	
	//plain copying
	if (dest_f.type == regfile || dest_f.type == nofile) {
		ret = file_to_file_copy (&source_f, &dest_f);
	}
	
	//file to dir copying
	if (dest_f.type == dir) {
		ret = file_to_dir_copy (&source_f, &dest_f);
	}
	//file to dir finish

	close (source_f.fd);
	if (!ret) close (dest_f.fd);
	return ret;
}

int file_to_dir_copy (struct fileinfo *source_f, struct fileinfo *dest_f)
{
	int ret = 0;
	//start forming new name
	char *source_filename = strrchr (source_f->name, '/');
	if (source_filename == NULL)
		source_filename = source_f->name;
	else source_filename += 1; 
		// now source_filname contains bare name of source file or "\0"
	
		//directory may end on '/' or no '/'
	char *last_slash = strrchr (dest_f->name, '/');
	if (last_slash != NULL) {
		if (*(last_slash + 1) == 0) 
			*last_slash = 0;
	}
		//now dest_f.name ends not on '/'
	
	char *dest_filename = (char *) calloc (strlen(dest_f->name) + 1 + strlen(source_filename) + 1, sizeof (char)); // one more +1 for slash
	strcpy (dest_filename, dest_f->name);
	strcat (dest_filename, "/");
	strcat (dest_filename, source_filename);
		//printf ("formed filename: '%s'\n", dest_filename);
	dest_f->name = dest_filename;
	//end forming new name
	
	ret = file_to_file_copy (source_f, dest_f);
	free (dest_filename);
	return ret;
}

int file_to_file_copy (struct fileinfo *source_f, struct fileinfo *dest_f)
{
	//returns -2 on wrong arg, ret of open_fileinfo or fcopy_mmap on other errors.
	if (source_f == NULL || dest_f == NULL)
		return -2;
	int ret = 0;
	ret = open_fileinfo (dest_f, O_RDWR | O_CREAT | O_EXCL, source_f->st.st_mode);
	if (ret == -1 && errno == EEXIST) {
		//here implementation of --force flag with reopening => ret changing
	       	return _ERR_FIXME;
	}
	if (ret)
		return ret;

	ret = fcopy_mmap (source_f, dest_f);
	if (ret)
		printf ("err: fcopy () returned %d\n", ret);
	return ret;
}

int open_fileinfo (struct fileinfo *file_f, int flags, mode_t mode)
{
	// returns -2 on wrong arg, -1 on open error, reports error to stderr and sets errno accordingly
	if (file_f == NULL)
		return -2;
	if (file_f->name == NULL)
		return -2;
	if (mode == no_mode)
		file_f->fd = open (file_f->name, flags);
	else
		file_f->fd = open (file_f->name, flags, mode);

	if (file_f->fd  == -1) {
		perror (file_f->name);
		return -1;
	}
	
	//stat_filenifo (file_f); - is equal to latter code, but is longer due to using filename, not file descriptor.

	fstat (file_f->fd, &(file_f->st));
	switch (file_f->st.st_mode & S_IFMT) {
		case S_IFREG:
			file_f->type = regfile;
			break;
		case S_IFDIR:
			file_f->type = dir;
			break;
		default:
			file_f->type = nosupport;
			break;
	}
	return 0;
}

int stat_fileinfo (struct fileinfo *file_f)
{
	if (file_f == NULL)
		return -2;
	if (file_f->name == NULL)
		return -2;
	if (stat (file_f->name, &(file_f->st))) {
		file_f->type = nofile;
		return -1;
	}
	switch (file_f->st.st_mode & S_IFMT) {
		case S_IFREG:
			file_f->type = regfile;
			break;
		case S_IFDIR:
			file_f->type = dir;
			break;
		default:
			file_f->type = nosupport;
			break;
	}
	return 0;
}

int fcopy_mmap (struct fileinfo *source_f, struct fileinfo *dest_f)
{
	/* basically copies file@fd to file@fd */
	
	#define SOURCE_SIZE source_f->st.st_size
	#define DEST_SIZE dest_f->st.st_size

	char *source_mmap = NULL;
	if ((source_mmap = (char *) mmap (NULL, SOURCE_SIZE, PROT_READ, MAP_SHARED, source_f->fd, 0)) == MAP_FAILED)
		return _ERR_MAPPING_SOURCE;

	if ((lseek (dest_f->fd, SOURCE_SIZE - 1, SEEK_SET) < 0) || (write (dest_f->fd, "", 1) != 1)) //set output file size
		return _ERR_IO_WRITE;

	char *dest_mmap = NULL;
	if ((dest_mmap = (char *) mmap (NULL, SOURCE_SIZE, PROT_WRITE, MAP_SHARED, dest_f->fd, 0)) == MAP_FAILED)
		return _ERR_MAPPING_DEST;
	memcpy (dest_mmap, source_mmap, SOURCE_SIZE);

	#undef SOURCE_SIZE
	#undef DEST_SIZE
	return 0;
}


/*
int fcopy_calloc (struct fileinfo *source_f, struct fileinfo *dest_f, off_t bs)
{
	//basically copies file@fd to file@fd 
	
	#define SOURCE_SIZE source_f->st.st_size
	#define DEST_SIZE dest_f->st.st_size

	if (SOURCE_SIZE < bs) bs = SOURCE_SIZE;

	char *buffer = (char *) calloc (bs, sizeof (char));
	size_t read_bytes = 0;
	while ((read_bytes = read (source_f->fd, buffer, bs)) != 0) {
		//unimportant
		switch (errno) {
			case EIO:
				free (buffer);
				return _ERR_IO_READ;
		}

		write (dest_f->fd, buffer, read_bytes);
		//unimportant
		switch (errno) {
			case EIO:
				free (buffer);
				return _ERR_IO_WRITE;
		}
	}
	free (buffer);

	#undef SOURCE_SIZE
	#undef DEST_SIZE
	return 0;
}
*/
