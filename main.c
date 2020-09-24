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

#define _ERR_FIXME 666 //typical return on lack of implementation

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

int fcopy_calloc (struct fileinfo *source_f, struct fileinfo *dest_f, off_t bs);
int lstat_fileinfo (struct fileinfo *file_f);
int file_to_file_copy (struct fileinfo *source_f, struct fileinfo *dest_f);
int file_to_file_copy_at (struct fileinfo *dir_f, struct fileinfo *source_f, struct fileinfo *dest_f);
int file_to_dir_copy (struct fileinfo *source_f, struct fileinfo *dest_f);

int symlink_to_file_copy (struct fileinfo *source_f, struct fileinfo *dest_f);
int symlink_to_file_copy_at (struct fileinfo *dir_f, struct fileinfo *source_f, struct fileinfo *dest_f);
int symlink_to_dir_copy (struct fileinfo *source_f, struct fileinfo *dir_f);


int main (int argc, char *argv[])
{
	//arg checking
	if (argc != 3) {
		printf ("Usage: %s <source> <destenition>\n", argv[0]);
		return -2;
	}
	const char nosupport_msg[] = "is not a regular file, symlink or directory";
	
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

	//source prcessing
	if (source_f.type == nosupport) {
		printf ("'%s': %s\n", source_f.name, nosupport_msg);
		return -2;
	}
	source_f.fd = open (source_f.name, O_RDONLY);
	if (source_f.fd == -1) {
		perror (source_f.name);
		return -1;
	}
	//dest filetype processing
	switch (dest_f.type) {
		case dir:
			switch (source_f.type) {
				case slink:
					ret = symlink_to_dir_copy (&source_f, &dest_f);
					break;
				case regfile:
					ret = file_to_dir_copy (&source_f, &dest_f);
					break;
				case dir:
					printf ("dir to dir fixme\n");
					//ret = dir_to_dir_copy (&source_f, &dest_f);
					break;
				default:
					break;
			}
			break;
		case slink:
			//must execute regfile section
		case nofile:
			//must execute regfile section
		case regfile:
			switch (source_f.type) {
				case slink:
					ret = symlink_to_file_copy (&source_f, &dest_f);
					break;
				case regfile:
					ret = file_to_file_copy (&source_f, &dest_f);
					break;
				case dir:
					printf ("dir to file copying is not supported, maybe you wish to use 'tar' instead?\n");
					break;
				default: 
					break;
			}
			break;
		case nosupport:
			printf ("'%s': %s\n", dest_f.name, nosupport_msg);
			ret = -2;
			break;
		default:
			break;
	}

	close (source_f.fd);
	if (!ret) close (dest_f.fd);
	return ret;
}

int symlink_to_file_copy (struct fileinfo *source_f, struct fileinfo *dest_f)
{
	#define NAME_LENGTH 4096

	if (source_f == NULL || dest_f == NULL)
		return -2;
	if (dest_f->type != nofile) {
		printf ("FIXME::EXISTS\n");
	       	return _ERR_FIXME;
	}
	char linked_name[NAME_LENGTH] = {0};
	if (readlink (source_f->name, linked_name, NAME_LENGTH) == -1) {
		perror (source_f->name);
		return -1;
	}
	if (symlink (linked_name, dest_f->name) == -1) {
		perror (dest_f->name);
		return -1;
	}
	return 0;
	
	#undef NAME_LENGTH
}

int symlink_to_file_copy_at (struct fileinfo *dir_f, struct fileinfo *source_f, struct fileinfo *dest_f)
{
	#define NAME_LENGTH 4096

	if (source_f == NULL || dest_f == NULL || dir_f == NULL)
		return -2;
	//dir open
	dir_f->fd = open (dir_f->name, O_RDONLY | O_DIRECTORY);
	if (dir_f->fd == -1) {
		perror (dir_f->name);
		return -1;
	}

	struct stat temp;
	if (!fstatat (dir_f->fd, dest_f->name, &temp, AT_SYMLINK_NOFOLLOW)) {
		printf ("FIXME::EXISTS\n");
	       	return _ERR_FIXME;
	}

	char linked_name[NAME_LENGTH] = {0};
	if (readlink (source_f->name, linked_name, NAME_LENGTH) == -1) {
		perror (source_f->name);
		return -1;
	}
	if (symlinkat (linked_name, dir_f->fd, dest_f->name) == -1) {
		perror (dest_f->name);
		return -1;
	}
	return 0;
	
	#undef NAME_LENGTH
}


int symlink_to_dir_copy (struct fileinfo *source_f, struct fileinfo *dir_f)
{
	// wrap, as file to dir copy can cope with it.
	return file_to_dir_copy (source_f, dir_f);
}


int file_to_dir_copy (struct fileinfo *source_f, struct fileinfo *dir_f)
{
	if (source_f == NULL || dir_f == NULL)
		return -2;
	int ret = 0;
	//start forming new name
	char *source_filename = strrchr (source_f->name, '/');
	if (source_filename == NULL)
		source_filename = source_f->name;
	else source_filename += 1;
		// now source_filname contains bare name of source file

	struct fileinfo dest_f = {0};
	dest_f.name = source_filename;
	dest_f.type = source_f->type;
		
	if (dest_f.type == slink)
		ret = symlink_to_file_copy_at (dir_f, source_f, &dest_f);
	else
		ret = file_to_file_copy_at (dir_f, source_f, &dest_f);
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
	if (source_f == NULL || dest_f == NULL || dir_f == NULL)
		return -2;
	//dir open
	dir_f->fd = open (dir_f->name, O_RDONLY | O_DIRECTORY);
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
