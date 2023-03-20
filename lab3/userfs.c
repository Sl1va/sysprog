#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "userfs.h"

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

	size_t size;
	bool to_del;
};

/** List of all files. */
static struct file *file_list = NULL;

#define for_each_file(item)  \
    for (struct file *item = file_list; item != NULL; item = item->next)

/* helpful file operations */
static struct file *find_file(const char *name);

static void add_file(struct file *f);

static void remove_file(struct file *f);

static int verify_fd(int fd);

static struct block *add_block(struct file *f);

struct filedesc {
	struct file *file;

	int flags;
	size_t offset;
};

/* helpful descriptor operations */
static int push_descriptor(struct filedesc *desc);

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
	struct file *f = find_file(filename);
	if (!f && !(flags & UFS_CREATE))
		throw_err(UFS_ERR_NO_FILE);
	
	// in case of opening file in write mode  we should
	// recreate file in order to be able to 
	// overwrite data
	if (f && (flags & UFS_WRITE_ONLY)) {
		remove_file(f);
		f = NULL;
	}

	if (!f) {
		// create file here
		f = (struct file *) malloc(sizeof(struct file));
		*f = (struct file) {
			.block_list = NULL,
			.last_block = NULL,
			.name = strdup(filename),
			.next = NULL,
			.prev = NULL,
			.refs = 0,
			.size = 0,
			.to_del = false,
		};

		add_file(f);
	}

	++(f->refs);

	struct filedesc *desc = (struct filedesc*) malloc(sizeof(struct filedesc));
	*desc = (struct filedesc) {
		.file = f,
		.flags = flags,
		.offset = 0,
	};

	return push_descriptor(desc);
}

ssize_t
ufs_write(int _fd, const char *buf, size_t size)
{
	int fd_err;
	if (fd_err = verify_fd(_fd), fd_err)
		throw_err(fd_err);

	struct filedesc *fd = file_descriptors[_fd];

	if (fd->flags & UFS_READ_ONLY)
		throw_err(UFS_ERR_NO_PERMISSION); 
	
	struct file *f = fd->file;
	if (!f->last_block)
		add_block(f);

	struct block *lb = f->last_block;
	size_t written = 0;

	while (written < size) {
		if (lb->occupied == BLOCK_SIZE)
			lb = add_block(f);

		size_t to_write = BLOCK_SIZE;

		// check if data to write is less than block size
		if (to_write > size - written)
				to_write = size - written;
		
		// check if current block can fit the size
		if (to_write > BLOCK_SIZE - lb->occupied)
				to_write = BLOCK_SIZE - lb->occupied;

		if (f->size + to_write > MAX_FILE_SIZE)
			break;

		memcpy(lb->memory + lb->occupied, buf + written, to_write);
		
		f->last_block->occupied += to_write;
		f->size += to_write;
		written += to_write;
	}

	if (written < size && f->size == MAX_FILE_SIZE)
		throw_err(UFS_ERR_NO_MEM);

	return written;
}

ssize_t
ufs_read(int _fd, char *buf, size_t size)
{
	int fd_err;
	if (fd_err = verify_fd(_fd), fd_err)
		throw_err(fd_err);

	struct filedesc *fd = file_descriptors[_fd];

	if (fd->flags & UFS_WRITE_ONLY)
		throw_err(UFS_ERR_NO_PERMISSION);
	
	struct block *cur = fd->file->block_list;
	if (!cur)
		return 0;
	
	// move to poiner offset
	for (int i = 0; i < fd->offset / BLOCK_SIZE; ++i, cur = cur->next)
		if (!cur) return 0;

	size_t block_offset = fd->offset % BLOCK_SIZE;
	size_t done = 0;

	while (done < size) {
		if (block_offset == BLOCK_SIZE) {
			cur = cur->next;
			block_offset = 0;

			if (!cur) break;
		}

		if (cur->occupied - block_offset == 0)
			break;

		size_t to_read = BLOCK_SIZE - block_offset;
		
		if (to_read > size - done)
			to_read = size - done;
		
		if (to_read > cur->occupied - block_offset)
			to_read = cur->occupied - block_offset;
		
		memcpy(buf + done, cur->memory + block_offset, to_read);

		fd->offset += to_read;
		done += to_read;
		block_offset += to_read;
	}

	return done;
}

int
ufs_close(int fd)
{
	int fd_err;
	if (fd_err = verify_fd(fd), fd_err)
		throw_err(fd_err);

	// decrement reference counter
	struct file *f = file_descriptors[fd]->file;
	--(f->refs);

	// if there are no more reference,
	// perform delayed deletion if needed
	if (!f->refs && f->to_del)
		remove_file(f);

	free(file_descriptors[fd]);
	file_descriptors[fd] = NULL;
	--file_descriptor_count;

	// if file descriptor was at the end of the list,
	// we can decrease list size
	while (file_descriptors && file_descriptor_capacity > 0
							&& !file_descriptors[file_descriptor_capacity - 1])
		free(file_descriptors[--file_descriptor_capacity]);
	
	file_descriptors = realloc(file_descriptors,
							  sizeof(struct filedesc *) * (file_descriptor_capacity));
	return 0;
}

int
ufs_delete(const char *filename)
{
	struct file *f = find_file(filename);
	if (!f)
		throw_err(UFS_ERR_NO_FILE);
	
	if (f->refs) {
		// file becomes invisible for opening, but
		// it still exists in memory until last reference is closed
		f->to_del = true;
	} else {
		remove_file(f);
	}
	return 0;
}

int
ufs_resize(int _fd, size_t new_size)
{
	int fd_err;
	if (fd_err = verify_fd(_fd), fd_err)
		throw_err(fd_err);

	if (new_size >= MAX_FILE_SIZE)
		throw_err(UFS_ERR_NO_MEM);

	struct filedesc *fd = file_descriptors[_fd];
	struct file *f = fd->file;

	int num_blocks = 0;
	for_each_block(f->block_list, _) ++num_blocks;
	
	while (num_blocks * BLOCK_SIZE < new_size) {
		add_block(f);
		++num_blocks;
	}
	
	if (num_blocks)
		while ((num_blocks - 1) * BLOCK_SIZE > new_size) {
			if (f->last_block->prev) {
				struct block *b = f->last_block->prev;

				free(b->next->memory);
				free(b->next);
				b->next = NULL;

				f->last_block = b;
			}
			--num_blocks;
		}

	f->size = new_size;
	return 0;
}


/* private helpful methods */
static struct file *find_file(const char *name) {
	struct file *f = NULL;
	for_each_file(i)
		if (!i->to_del && !strcmp(i->name, name))
			f = i;

	return f;
}

static void add_file(struct file *f) {
	if (!file_list) {
		file_list = f;
	} else {
		struct file *last_file = NULL;
		for_each_file(_f)
			last_file = _f;

		last_file->next = f;
		f->prev = last_file;
	}
}

static void remove_file(struct file *f) {
	// free file blocks
	struct block *last_block = NULL;
	for_each_block(f->block_list, b) {
		free(b->memory);
		if (b->prev)
			free(b->prev);

		last_block = b;
	}
	free(last_block);
	free((void *)f->name);

	// fix file list connections
	if (f->next)
		f->next->prev = f->prev;
	
	if (f->prev)
		f->prev->next = f->next;
	

	if (file_list == f)
		file_list = f->next;
	
	// free file instance itself
	free(f);
}

static int verify_fd(int fd) {
	// check if file descriptor valid
	if (fd < 0 || fd >= file_descriptor_capacity)
		return UFS_ERR_NO_FILE;

	if (!file_descriptors[fd])
		return UFS_ERR_NO_FILE;

	return UFS_ERR_NO_ERR;
}

static struct block *add_block(struct file *f) {
	struct block *new_block = (struct block *) malloc(sizeof(struct block));
	char *mem = (char *) malloc(sizeof(char) * BLOCK_SIZE);

	*new_block = (struct block) {
		.memory = mem,
		.prev = NULL,
		.next = NULL,
		.occupied = 0,
	};

	if (!f->block_list) {
		f->block_list = new_block;	
		f->last_block = new_block;
	} else {
		f->last_block->next = new_block;
		new_block->prev = f->last_block;
		f->last_block = new_block;
	}

	return new_block;
}

static int push_descriptor(struct filedesc *desc) {
	int available_fd = -1;

	if (!file_descriptor_capacity) {
		file_descriptors = realloc(file_descriptors,
									sizeof(struct filedesc *) * (++file_descriptor_capacity));
		file_descriptors[0] = NULL;
	}

	for (int i = file_descriptor_capacity - 1; i >= 0; --i) {
		if (!file_descriptors[i])
			available_fd = i;
	}

	if (available_fd != -1)
		goto end;

	// in other case create new descriptor
	// and add it to the end of the list
	file_descriptors = realloc(file_descriptors,
								sizeof(struct filedesc *) * (++file_descriptor_capacity));
	available_fd = file_descriptor_capacity - 1;

end:
	++file_descriptor_count;
	file_descriptors[available_fd] = desc;
	return available_fd;
}