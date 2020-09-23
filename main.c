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
const char nosupport_msg[] = "is not a regular file, symlink or directory";

typedef int fdesc;

enum filetype {
	nofile,
	dir,
	regfile,
	slink,
	nosupport
};

struct fileinfo {
	char *name;
	struct stat st;
	fdesc fd;
	enum filetype type;
};

//int fcopy_mmap (struct fileinfo *source_f, struct fileinfo *dest_f);
int fcopy_calloc (struct fileinfo *source_f, struct fileinfo *dest_f, off_t bs);
int lstat_fileinfo (struct fileinfo *file_f);
int file_to_file_copy (struct fileinfo *source_f, struct fileinfo *dest_f);
int file_to_file_copy_at (struct fileinfo *dir_f, struct fileinfo *source_f, struct fileinfo *dest_f);
int file_to_dir_copy (struct fileinfo *source_f, struct fileinfo *dest_f);


int main (int argc, char *argv[])
{
	//arg checking
	if (argc != 3) {
		printf ("Usage: %s <source> <destenition>\n", argv[0]);
		return _ERR_ARG_COUNT;
	}
	int ret = 0; //common var for return value

	//sorce stat
	struct fileinfo source_f = {0};
	source_f.name = argv[1];
	ret = lstat_fileinfo (&source_f);
	if (ret) {
		perror (source_f.name);
		return ret;
	}
	
	//dest stat
	struct fileinfo dest_f = {0};
	dest_f.name = argv[2];
	lstat_fileinfo (&dest_f);

	//source filetype prcessing
	if (source_f.type == nosupport) {
		//recursive copying will be somewhat here
		printf ("'%s': %s\n", source_f.name, nosupport_msg);
		return _ERR_ARG_TYPE;
	}
	if (source_f.type == dir) {
		printf ("no support for dir to <dest> copying yet\n>");
		return _ERR_FIXME;
	}
	//source open
	source_f.fd = open (source_f.name, O_RDONLY);
	if (source_f.fd == -1) {
		perror (source_f.name);
		return -1;
	}
	//dest filetype processing
	if (dest_f.type == nosupport) {
		printf ("'%s': %s\n", dest_f.name, nosupport_msg);
		return _ERR_ARG_TYPE;
	}
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

int file_to_dir_copy (struct fileinfo *source_f, struct fileinfo *dir_f)
{
	int ret = 0;
	//start forming new name
	char *source_filename = strrchr (source_f->name, '/');
	if (source_filename == NULL)
		source_filename = source_f->name;
	else source_filename += 1;
		// now source_filname contains bare name of source file
	
	/*
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
	*/
	struct fileinfo dest_f = {0};
	dest_f.name = source_filename;
	dest_f.type = source_f->type;
		
	ret = file_to_file_copy_at (dir_f, source_f, &dest_f);
	//free (dest_filename);
	return ret;
}

int file_to_file_copy (struct fileinfo *source_f, struct fileinfo *dest_f)
{
	//returns -2 on wrong arg, ret of open_fileinfo or fcopy_mmap on other errors.
	if (source_f == NULL || dest_f == NULL)
		return -2;
	dest_f->fd = open (dest_f->name, O_WRONLY | O_CREAT | O_EXCL, source_f->st.st_mode);
	if (dest_f->fd == -1 && errno == EEXIST) {
		//here implementation of --force flag with reopening => ret changing
		printf ("FIXME::EXISTS\n");
	       	return _ERR_FIXME;
	}
	if (dest_f->fd == -1) {
		perror (dest_f->name);
		return -1;
	}

	int ret = 0;
	ret = fcopy_calloc (source_f, dest_f, 32768);
	if (ret)
		printf ("err: fcopy () returned %d\n", ret);
	return ret;
}

int file_to_file_copy_at (struct fileinfo *dir_f, struct fileinfo *source_f, struct fileinfo *dest_f)
{
	//returns -2 on wrong arg, -1 or ret of fcopy_<SMTH> on other errors.
	if (source_f == NULL || dest_f == NULL || dir_f)
		return -2;
	//dir open
	dir_f->fd = open (dir_f->name, O_WRONLY);
	if (dir_f->fd == -1) {
		perror (dir_f->name);
		return -1;
	}
	//open dest
	dest_f->fd = openat (dir_f->fd, dest_f->name, O_WRONLY | O_CREAT | O_EXCL, source_f->st.st_mode);
	if (dest_f->fd == -1 && errno == EEXIST) {
		//here implementation of --force flag with reopening => ret changing
		printf ("FIXME::EXISTS\n");
		return _ERR_FIXME;
	}
	if (dest_f->fd == -1) {
		perror (dest_f->name);
		return -1;
	}

	int ret = 0;
	ret = fcopy_calloc (source_f, dest_f, 32768);
	if (ret)
		printf ("err: fcopy () returned %d\n", ret);
	return ret;
}


int lstat_fileinfo (struct fileinfo *file_f)
{
	if (file_f == NULL)
		return -2;
	if (file_f->name == NULL)
		return -2;
	if (lstat (file_f->name, &(file_f->st))) {
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
		case S_IFLNK:
			file_f->type = slink;
			break;
		default:
			file_f->type = nosupport;
			break;
	}
	return 0;
}

int fcopy_calloc (struct fileinfo *source_f, struct fileinfo *dest_f, off_t bs)
{
	// basically copies file@fd to file@fd 
	// reliable like swiss watch, slow
	
	#define SOURCE_SIZE source_f->st.st_size
	#define DEST_SIZE dest_f->st.st_size

	if (SOURCE_SIZE < bs) bs = SOURCE_SIZE;

	char *buffer = (char *) calloc (bs, sizeof (char));
	ssize_t read_bytes = 0;
	while ((read_bytes = read (source_f->fd, buffer, bs)) != 0) {
		if (read_bytes == (ssize_t) -1) {
			perror (source_f->name);
			return -1;
		}

		if (write (dest_f->fd, buffer, read_bytes) == -1) {
			perror (dest_f->name);
			return -1;
		}
	}
	free (buffer);

	#undef SOURCE_SIZE
	#undef DEST_SIZE
	return 0;
}

/*
int fcopy_mmap (struct fileinfo *source_f, struct fileinfo *dest_f)
{
	// basically copies file@fd to file@fd 
	
	#define SOURCE_SIZE source_f->st.st_size
	#define DEST_SIZE dest_f->st.st_size

	char *source_mmap = NULL;
	if ((source_mmap = (char *) mmap (NULL, SOURCE_SIZE, PROT_READ, MAP_SHARED, source_f->fd, 0)) == MAP_FAILED)
		return _ERR_MAPPING_SOURCE;

	if (ftruncate (dest_f->fd, SOURCE_SIZE)) //set output file size
		perror (dest_f->name);
		return _ERR_IO_WRITE;

	char *dest_mmap = NULL;
	if ((dest_mmap = (char *) mmap (NULL, SOURCE_SIZE, PROT_WRITE, MAP_SHARED, dest_f->fd, 0)) == MAP_FAILED)
		return _ERR_MAPPING_DEST;
	memcpy (dest_mmap, source_mmap, SOURCE_SIZE);

	#undef SOURCE_SIZE
	#undef DEST_SIZE
	return 0;
}
*/

