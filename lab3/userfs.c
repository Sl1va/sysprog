#include "userfs.h"
#include <stddef.h>

enum {
	BLOCK_SIZE = 512,
	MAX_FILE_SIZE = 1024 * 1024 * 1024,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

#define throw_err(err)          \
    do {                        \
        ufs_error_code = err;   \
        return -1;              \
    }while(0)


struct block {
	/** Block memory. */
	char *memory;
	/** How many bytes are occupied. */
	int occupied;
	/** Next block in the file. */
	struct block *next;
	/** Previous block in the file. */
	struct block *prev;

	/* PUT HERE OTHER MEMBERS */
};

#define for_each_block(list, item)  \
    for (struct block *item = list; item != NULL; item = item->next)


struct file {
	/** Double-linked list of file blocks. */
	struct block *block_list;
	/**
	 * Last block in the list above for fast access to the end
	 * of file.
	 */
	struct block *last_block;
	/** How many file descriptors are opened on the file. */
	int refs;
	/** File name. */
	const char *name;
	/** Files are stored in a double-linked list. */
	struct file *next;
	struct file *prev;

	/* PUT HERE OTHER MEMBERS */
};

/** List of all files. */
static struct file *file_list = NULL;

#define for_each_file(item)  \
    for (struct file *item = file_list; item != NULL; item = item->next)


struct filedesc {
	struct file *file;

	/* PUT HERE OTHER MEMBERS */
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code
ufs_errno()
{
	return ufs_error_code;
}

int
ufs_open(const char *filename, int flags)
{
	/* IMPLEMENT THIS FUNCTION */
	throw_err(UFS_ERR_NOT_IMPLEMENTED);
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
	throw_err(UFS_ERR_NOT_IMPLEMENTED);
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
	/* IMPLEMENT THIS FUNCTION */
	throw_err(UFS_ERR_NOT_IMPLEMENTED);
}

int
ufs_close(int fd)
{
	/* IMPLEMENT THIS FUNCTION */
	throw_err(UFS_ERR_NOT_IMPLEMENTED);
}

int
ufs_delete(const char *filename)
{
	/* IMPLEMENT THIS FUNCTION */
	throw_err(UFS_ERR_NOT_IMPLEMENTED);
}

int
ufs_resize(int fd, size_t new_size)
{
	/* IMPLEMENT THIS FUNCTION */
	throw_err(UFS_ERR_NOT_IMPLEMENTED);
}