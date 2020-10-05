#define _GNU_SOURCE
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
#include <dirent.h> 
#include <inttypes.h>
#include <sys/syscall.h>

extern int errno;

#define _ERR_FIXME 666 //typical return on lack of implementation

typedef int fdesc;

const char nosupport_msg[] = "is not a regular file, symlink or directory";
struct options {
	char force_flag;
	char uid_gid_copy_flag;
	char time_copy_flag;
	unsigned int getdents_buffer_size;
	off_t block_size;
} g_options = {0};

enum filetype {
	nofile,
	dir,
	regfile,
	slink,
	nosupport
};

struct fileinfo {
	const char *name;
	struct stat st;
	fdesc fd;
	fdesc parent_dirfd; //fd of parent directory
	enum filetype type;
};

struct fileinfo AT_FDCWD_f;

struct linux_dirent64 {
	ino64_t        d_ino;    /* 64-bit inode number */
	off64_t        d_off;    /* 64-bit offset to next structure */
	unsigned short d_reclen; /* Size of this dirent */
	unsigned char  d_type;   /* File type */
	char           d_name[]; /* Filename (null-terminated) */
};


int fcopy_calloc (struct fileinfo *source_f, struct fileinfo *dest_f, off_t bs);
int lstat_fileinfo (struct fileinfo *file_f);
int fstatat_fileinfo (struct fileinfo *dir_f, struct fileinfo *file_f, int flags);
int file_to_file_copy (struct fileinfo *source_f, struct fileinfo *dest_f);
int file_to_file_copy_at (struct fileinfo *dir_f, struct fileinfo *source_f, struct fileinfo *dest_f);
int file_to_dir_copy (struct fileinfo *source_f, struct fileinfo *dest_f);

int dir_to_dir_copy (struct fileinfo *source_f, struct fileinfo *dest_f);

int symlink_to_file_copy (struct fileinfo *source_f, struct fileinfo *dest_f);
int symlink_to_file_copy_at (struct fileinfo *dir_f, struct fileinfo *source_f, struct fileinfo *dest_f);
int symlink_to_dir_copy (struct fileinfo *source_f, struct fileinfo *dir_f);

int parse_options (int argc, char *argv[]);

int switch_source_to_regfile (struct fileinfo *source_f, struct fileinfo *dest_f);
int switch_source_to_dir (struct fileinfo *source_f, struct fileinfo *dest_f);
int switch_source_to_nofile (struct fileinfo *source_f, struct fileinfo *dest_f);
int switch_source_to_symlink (struct fileinfo *source_f, struct fileinfo *dir_f);
int switch_source_openat (struct fileinfo *dir_f, struct fileinfo *source_f, int stat_flags);
int switch_source_dest_resolve (struct fileinfo *source_f, struct fileinfo *dest_f);

int fileinfo_init (struct fileinfo *file_f, const char *name);

int main (int argc, char *argv[])
{
	int ret = 0; //common var for return value

	//arg checking
	if (argc < 3) {
		printf ("Usage: %s <options> <source> <destenition>\n", argv[0]);
		return -2;
	}
	ret = parse_options (argc, argv);
	if (ret)
		return ret;

	//sorce stat and open
	struct fileinfo source_f = {0};
	fileinfo_init (&source_f, argv[argc-2]);
	ret = switch_source_openat (&AT_FDCWD_f, &source_f, AT_SYMLINK_NOFOLLOW);
	if (ret)
		return ret;

	//dest stat
	struct fileinfo dest_f = {0};
	fileinfo_init (&dest_f, argv[argc-1]);
	fstatat_fileinfo (&AT_FDCWD_f, &dest_f, AT_SYMLINK_NOFOLLOW);

	//dest filetype processing
	ret = switch_source_dest_resolve (&source_f, &dest_f);

	close (source_f.fd);
	return ret;
}

//########        section wraps        ########//
int symlink_to_dir_copy (struct fileinfo *source_f, struct fileinfo *dir_f)
{
	return file_to_dir_copy (source_f, dir_f);
}

int switch_source_to_symlink (struct fileinfo *source_f, struct fileinfo *dir_f)
{
	return switch_source_to_regfile (source_f, dir_f);
}
//########      section wraps end      ########// 

//########      section switches       ########//
int switch_source_dest_resolve (struct fileinfo *source_f, struct fileinfo *dest_f)
{
	int ret = 0;
	switch (dest_f->type) {
		case dir:
			ret = switch_source_to_dir (source_f, dest_f);
			break;
		case slink:
			ret = switch_source_to_symlink (source_f, dest_f);
			break;
		case nofile:
			ret = switch_source_to_nofile (source_f, dest_f);
			break;
		case regfile:
			ret = switch_source_to_regfile (source_f, dest_f);
			break;
		case nosupport:
			printf ("'%s': %s\n", dest_f->name, nosupport_msg);
			ret = -2;
			break;
		default:
			break;
	}
	return ret;
}

int switch_source_openat (struct fileinfo *dir_f, struct fileinfo *source_f, int stat_flags)
{
	if (source_f == NULL || dir_f == NULL)
		return -2;
	int dir_opened_flag = 0;
	//dir open if not opened yet
	if (dir_f->fd == -1) {
		dir_f->fd = open (dir_f->name, O_RDONLY | O_DIRECTORY);
		dir_opened_flag = 1;
		if (dir_f->fd == -1) {
			perror (dir_f->name);
			return -1;
		}
	}
	//stat	
	if (fstatat_fileinfo (dir_f, source_f, stat_flags)) {
		perror (source_f->name);
		if (dir_opened_flag)
			close (dir_f->fd);
		return -1;
	}

	if (source_f->type == nosupport) {
		printf ("'%s': %s\n", source_f->name, nosupport_msg);
		if (dir_opened_flag)
			close (dir_f->fd);
		return -1;
	}
	if (source_f->type != slink) {
		source_f->fd = openat (dir_f->fd, source_f->name, O_RDONLY);
		if (source_f->fd == -1) {
			perror (source_f->name);
			if (dir_opened_flag)
				close (dir_f->fd);
			return -1;
		}
	}
	source_f->parent_dirfd = dir_f->fd;
	return 0;
}

int switch_source_to_dir (struct fileinfo *source_f, struct fileinfo *dest_f)
{
	int ret = 0;
	switch (source_f->type) {
		case slink:
			ret = symlink_to_dir_copy (source_f, dest_f);
			break;
		case regfile:
			ret = file_to_dir_copy (source_f, dest_f);
			break;
		case dir:
			ret = dir_to_dir_copy (source_f, dest_f);
			break;
		default:
			break;
	}
	return ret;
}

int switch_source_to_regfile (struct fileinfo *source_f, struct fileinfo *dest_f)
{
	int ret = 0;
	switch (source_f->type) {
		case slink:
			ret = symlink_to_file_copy (source_f, dest_f);
			break;
		case regfile:
			ret = file_to_file_copy (source_f, dest_f);
			break;
		case dir:
			printf ("dir to file copying is not supported, maybe you wish to use 'tar' instead?\n");
			break;
		default: 
			break;
	}
	return ret;
}

int switch_source_to_nofile (struct fileinfo *source_f, struct fileinfo *dest_f)
{
	if (source_f->type == dir) {
		return dir_to_dir_copy (source_f, dest_f);
	}
	return switch_source_to_regfile (source_f, dest_f);
}
//########    section switches end     ########//

int parse_options (int argc, char *argv[])
{
	g_options.getdents_buffer_size = 32768;
	g_options.block_size = 32768;
	AT_FDCWD_f.fd = AT_FDCWD;

	const char noparam_msg[] = "unrecognized option";
	for (int i = 1; i < argc - 2; i++) {
		if (argv[i][0] != '-') {
			printf ("%s: '%s'\n", noparam_msg, argv[i]);
			return -1;
		}
		for (int j = 1; argv[i][j] != 0; j++) {
			switch (argv[i][j]) {
				case 'f':
					g_options.force_flag = 1;
					break;
				case 'u':
					g_options.uid_gid_copy_flag = 1;
					break;
				case 't':
					g_options.time_copy_flag = 1;
					break;
				default:
					printf ("%s: '%s'\n", noparam_msg, argv[i]);
					return -1;
			}
		}
	}
	return 0;
}

int dir_to_dir_copy (struct fileinfo *source_f, struct fileinfo *dest_f)
{
	// dest is DIR! where we will copy entire source dir: source ->cp-> dest/source/...
	
	int dir_opened_flag = 0;
	if (source_f == NULL || dest_f == NULL)
		return -2;
	//dest open if not opened yet
	if (dest_f->fd == 0 || dest_f->fd == -1) {
		dest_f->fd = open (dest_f->name, O_RDONLY | O_DIRECTORY);
		dir_opened_flag = 1;
		if (dest_f->fd == -1) {
			perror (dest_f->name);
			return -1;
		}
	}
	//forming new cur_dest_f name (if source name is like ./test/dir/)
	const char *dest_filename = source_f->name;
	const char *tmp_filename = source_f->name;
	const char *tmp = NULL;
	while ((tmp = strchr (dest_filename, '/')) != NULL) {
		tmp_filename = dest_filename;
		dest_filename = tmp + 1;
	}
	if (*dest_filename == 0)
		dest_filename = tmp_filename;
	// now dest_filname contains bare name of source file, 'dir' or 'dir/'

	//stat and open cur_dest_dir
	struct fileinfo cur_dest_f = {0};
	fileinfo_init (&cur_dest_f, dest_filename);
	//mkdir cur_dest_f dir if not exists
	mkdirat (dest_f->fd, cur_dest_f.name, S_IRUSR | S_IWUSR | S_IXUSR);
	fstatat_fileinfo (dest_f, &cur_dest_f, 0);
	if (cur_dest_f.type == nosupport) {
		printf ("'%s': %s\n", cur_dest_f.name, nosupport_msg);
		if (dir_opened_flag)
			close (dest_f->fd);
		return -1;
	}
	cur_dest_f.fd = openat (dest_f->fd, cur_dest_f.name, O_RDONLY | O_DIRECTORY);
	//overwrie at --force flag
	if (cur_dest_f.fd == -1 && g_options.force_flag) {
		if (unlinkat (dest_f->fd, cur_dest_f.name, 0)) {
			perror (cur_dest_f.name);
			if (dir_opened_flag)
				close (dest_f->fd);
			return -1;
		}
		mkdirat (dest_f->fd, cur_dest_f.name, S_IRUSR | S_IWUSR | S_IXUSR);
		cur_dest_f.fd = openat (dest_f->fd, cur_dest_f.name, O_RDONLY | O_DIRECTORY);	
	}
	if (cur_dest_f.fd == -1) {
		perror (cur_dest_f.name);
		if (dir_opened_flag)
			close (dest_f->fd);
		return -1;
	}

	//getdents & copy
	char getdents_buffer[g_options.getdents_buffer_size];
	struct linux_dirent64 *d_entry = (struct linux_dirent64 *) getdents_buffer;

	while (1) {
		int read_bytes = syscall (SYS_getdents64, source_f->fd, getdents_buffer, g_options.getdents_buffer_size);
		if (read_bytes == -1) {
			perror (source_f->name);
			if (dir_opened_flag)
				close (dest_f->fd);
			return -1;
		}
		if (!read_bytes)
			break;
		//disemboweling dir, disemboweling completely ;)
		for (int cur_pos = 0; cur_pos < read_bytes; cur_pos += d_entry->d_reclen) {
			d_entry = (struct linux_dirent64 *) (getdents_buffer + cur_pos);
			//getdents64 takes . and .. for real folders
				//printf ("processing '%s' and current pisition is '%d' and sizeof 64struct is '%ld'\n", d_entry->d_name, cur_pos, sizeof (*d_entry));
			if (!strcmp (d_entry->d_name, ".") || !strcmp (d_entry->d_name, ".."))
				continue;
			//stat and open current file to copy
			struct fileinfo cur_source_f = {0};
			fileinfo_init (&cur_source_f, d_entry->d_name);
			if (switch_source_openat (source_f, &cur_source_f, AT_SYMLINK_NOFOLLOW)) {
				printf ("skipping\n");
				continue;
			}
			if (switch_source_to_dir (&cur_source_f, &cur_dest_f)) {
				//mb copy error handle
			}
			close (cur_source_f.fd);
		}
	}
	//set rights
	if (fchmod (cur_dest_f.fd, source_f->st.st_mode)) {
		perror (cur_dest_f.name);
		if (dir_opened_flag)
			close (dest_f->fd);
		return -1;
	}

	if (g_options.uid_gid_copy_flag) {
		if (fchownat (dest_f->fd, cur_dest_f.name, source_f->st.st_uid, source_f->st.st_gid, AT_SYMLINK_NOFOLLOW)) {
			perror (cur_dest_f.name);
			if (dir_opened_flag)
				close (dest_f->fd);
			return -1;
		}
	}
	if (g_options.time_copy_flag) {
		const struct timespec time[2] = {source_f->st.st_atim, source_f->st.st_mtim};
		if (utimensat (dest_f->fd, cur_dest_f.name, time, 0)) {
			perror (cur_dest_f.name);
			if (dir_opened_flag)
				close (dest_f->fd);
			return -1;
		}
	}
	if (dir_opened_flag)
		close (dest_f->fd);
	return 0;
}

int symlink_to_file_copy (struct fileinfo *source_f, struct fileinfo *dest_f)
{
	#define NAME_LENGTH 4096

	if (source_f == NULL || dest_f == NULL)
		return -2;
	if (dest_f->type != nofile && g_options.force_flag) {
		if (unlink (dest_f->name)) {
			perror (dest_f->name);
			return -1;
		}
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
	if (g_options.uid_gid_copy_flag) {
		if (lchown (dest_f->name, source_f->st.st_uid, source_f->st.st_gid)) {
			perror (dest_f->name);
			return -1;
		}
	}
	
	if (g_options.time_copy_flag) {
		const struct timespec time[2] = {source_f->st.st_atim, source_f->st.st_mtim};
		if (utimensat (AT_FDCWD, dest_f->name, time, AT_SYMLINK_NOFOLLOW)) {
			perror (dest_f->name);
			return -1;
		}
	}
	return 0;
	
	#undef NAME_LENGTH
}

int symlink_to_file_copy_at (struct fileinfo *dir_f, struct fileinfo *source_f, struct fileinfo *dest_f)
{
	#define NAME_LENGTH 4096

	if (source_f == NULL || dest_f == NULL || dir_f == NULL || dir_f->fd == -1) 
		return -2;

	//overwrite at --force flag
	struct stat temp;
	if (!fstatat (dir_f->fd, dest_f->name, &temp, AT_SYMLINK_NOFOLLOW) && g_options.force_flag) {
		if (unlinkat (dir_f->fd, dest_f->name, 0)) {
			perror (dest_f->name);
			return -1;
		}
	}

	char linked_name[NAME_LENGTH] = {0};
	if (readlinkat (source_f->parent_dirfd, source_f->name, linked_name, NAME_LENGTH) == -1) {
		perror (source_f->name);
		return -1;
	}
	if (symlinkat (linked_name, dir_f->fd, dest_f->name) == -1) {
		perror (dest_f->name);
		return -1;
	}
	if (g_options.uid_gid_copy_flag) {
		if (fchownat (dir_f->fd, dest_f->name, source_f->st.st_uid, source_f->st.st_gid, AT_SYMLINK_NOFOLLOW)) {
			perror (dest_f->name);
			return -1;
		}
	}
	if (g_options.time_copy_flag) {
		const struct timespec time[2] = {source_f->st.st_atim, source_f->st.st_mtim};
		if (utimensat (dir_f->fd, dest_f->name, time, AT_SYMLINK_NOFOLLOW)) {
			perror (dest_f->name);
			return -1;
		}
	}
	return 0;
	
	#undef NAME_LENGTH
}

int file_to_dir_copy (struct fileinfo *source_f, struct fileinfo *dir_f)
{
	if (source_f == NULL || dir_f == NULL)
		return -2;
	int ret = 0;
	int dir_opened_flag = 0;
	//forming new dest_f name
	const char *dest_filename = source_f->name;
	const char *tmp_filename = source_f->name;
	const char *tmp = NULL;
	while ((tmp = strchr (dest_filename, '/')) != NULL) {
		tmp_filename = dest_filename;
		dest_filename = tmp + 1;
	}
	if (*dest_filename == 0)
		dest_filename = tmp_filename;
	// now dest_filname contains bare name of source file, 'file' or 'file/'

	struct fileinfo dest_f = {0};
	fileinfo_init (&dest_f, dest_filename);
	dest_f.type = source_f->type;

	//dir open if not opened yet
	if (dir_f->fd == -1) {
		dir_f->fd = open (dir_f->name, O_RDONLY | O_DIRECTORY);
		dir_opened_flag = 1;
		if (dir_f->fd == -1) {
			perror (dir_f->name);
			return -1;
		}
	}
		
	if (dest_f.type == slink)
		ret = symlink_to_file_copy_at (dir_f, source_f, &dest_f);
	else
		ret = file_to_file_copy_at (dir_f, source_f, &dest_f);
	if (dir_opened_flag)
		close (dir_f->fd);
	return ret;
}

int file_to_file_copy (struct fileinfo *source_f, struct fileinfo *dest_f)
{
	//returns -2 on wrong arg, ret of open_fileinfo or fcopy_mmap on other errors.
	if (source_f == NULL || dest_f == NULL)
		return -2;
	dest_f->fd = open (dest_f->name, O_WRONLY | O_CREAT | O_EXCL, source_f->st.st_mode);
	//overwrite at --force flag
	if (dest_f->fd == -1 && errno == EEXIST && g_options.force_flag) {
		dest_f->fd = open (dest_f->name, O_WRONLY | O_TRUNC, source_f->st.st_mode);
		if (dest_f->fd == -1) {
			if (unlink (dest_f->name)) {
				perror (dest_f->name);
				return -1;
			}
			dest_f->fd = open (dest_f->name, O_WRONLY | O_CREAT, source_f->st.st_mode);
		}
	}
	if (dest_f->fd == -1) {
		perror (dest_f->name);
		return -1;
	}

	int ret = 0;
	ret = fcopy_calloc (source_f, dest_f, g_options.block_size);
	if (ret) {
		printf ("err: fcopy () returned %d\n", ret);
		close (dest_f->fd);
		return ret;
	}

	if (g_options.uid_gid_copy_flag) {
		if (fchown (dest_f->fd, source_f->st.st_uid, source_f->st.st_gid)) {
			perror (dest_f->name);
			close (dest_f->fd);
			return -1;
		}
	}
	if (g_options.time_copy_flag) {
		const struct timespec time[2] = {source_f->st.st_atim, source_f->st.st_mtim};
		if (futimens (dest_f->fd, time)) {
			perror (dest_f->name);
			close (dest_f->fd);
			return -1;
		}
	}
	close (dest_f->fd);
	return 0;
}

int file_to_file_copy_at (struct fileinfo *dir_f, struct fileinfo *source_f, struct fileinfo *dest_f)
{
	//returns -2 on wrong arg, -1 or ret of fcopy_<SMTH> on other errors.
	if (source_f == NULL || dest_f == NULL || dir_f == NULL || dir_f->fd == -1)
		return -2;
	//open dest
	dest_f->fd = openat (dir_f->fd, dest_f->name, O_WRONLY | O_CREAT | O_EXCL, source_f->st.st_mode);
	//overwrie at --force flag
	if (dest_f->fd == -1 && errno == EEXIST && g_options.force_flag) {
		dest_f->fd = openat (dir_f->fd, dest_f->name, O_WRONLY | O_TRUNC, source_f->st.st_mode);
		if (dest_f->fd == -1) {
			if (unlinkat (dir_f->fd, dest_f->name, 0)) {
				perror (dest_f->name);
				return -1;
			}
			dest_f->fd = openat (dir_f->fd, dest_f->name, O_WRONLY | O_CREAT, source_f->st.st_mode);
		}
	}
	if (dest_f->fd == -1) {
		perror (dest_f->name);
		return -1;
	}

	int ret = 0;
	ret = fcopy_calloc (source_f, dest_f, g_options.block_size);
	if (ret) {
		printf ("err: fcopy () returned %d\n", ret);
		close (dest_f->fd);
		return ret;
	}

	if (g_options.uid_gid_copy_flag) {
		if (fchown (dest_f->fd, source_f->st.st_uid, source_f->st.st_gid)) {
			perror (dest_f->name);
			close (dest_f->fd);
			return -1;
		}
	}
	if (g_options.time_copy_flag) {
		const struct timespec time[2] = {source_f->st.st_atim, source_f->st.st_mtim};
		if (futimens (dest_f->fd, time)) {
			perror (dest_f->name);
			close (dest_f->fd);
			return -1;
		}
	}
	close (dest_f->fd);
	return 0;
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

int fstatat_fileinfo (struct fileinfo *dir_f, struct fileinfo *file_f, int flags)
{
	if (file_f == NULL || dir_f == NULL)
		return -2;
	if (file_f->name == NULL)
		return -2;
	int dir_opened_flag = 0;
	//dir open if not opened yet
	if (dir_f->fd == -1) {
		dir_f->fd = open (dir_f->name, O_RDONLY | O_DIRECTORY);
		dir_opened_flag = 1;
		if (dir_f->fd == -1) {
			perror (dir_f->name);
			return -1;
		}
	}
	if (fstatat (dir_f->fd, file_f->name, &(file_f->st), flags)) {
		file_f->type = nofile;
		if (dir_opened_flag)
			close (dir_f->fd);
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
	if (dir_opened_flag)
		close (dir_f->fd);
	return 0;
}


int fcopy_calloc (struct fileinfo *source_f, struct fileinfo *dest_f, off_t bs)
{
	// basically copies file@fd to file@fd 
	// reliable like swiss watch, slow
	
	#define SOURCE_SIZE source_f->st.st_size
	#define DEST_SIZE dest_f->st.st_size

	if (source_f == NULL || dest_f == NULL)
		return -2;
	if (source_f->fd == -1 || dest_f->fd == -1)
		return -2;

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

int fileinfo_init (struct fileinfo *file_f, const char *name)
{
	file_f->name = name;
	file_f->type = nosupport;
	file_f->fd = -1;
	file_f->parent_dirfd = -1;
	return 0;
}
