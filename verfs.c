#include <linux/module.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/security.h>
#include <linux/namei.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>

#define VERFS_VERSION "0.1"

long * sys_call_table = (long *)SYS_CALL_TABLE_ADDR;
	
/* saved syscalls */
asmlinkage long (*linux_sys_unlink) (const char * pathname);
asmlinkage long (*linux_sys_open) (const char * filename, int flags, int mode);

/* pointers to non-exported system calls */
asmlinkage long (*linux_sys_rename) (const char * oldname, const char * newname);

/* Duplicated from kernel */
static inline int do_rename(const char * oldname, const char * newname)
{
	int error = 0;
	struct dentry * old_dir, * new_dir;
	struct dentry * old_dentry, *new_dentry;
	struct dentry * trap;
	struct nameidata oldnd, newnd;

	error = path_lookup(oldname, LOOKUP_PARENT, &oldnd);
	if (error)
		goto exit;

	error = path_lookup(newname, LOOKUP_PARENT, &newnd);
	if (error)
		goto exit1;

	error = -EXDEV;
	if (oldnd.mnt != newnd.mnt)
		goto exit2;

	old_dir = oldnd.dentry;
	error = -EBUSY;
	if (oldnd.last_type != LAST_NORM)
		goto exit2;

	new_dir = newnd.dentry;
	if (newnd.last_type != LAST_NORM)
		goto exit2;

	trap = lock_rename(new_dir, old_dir);

	old_dentry = lookup_hash(&oldnd.last, old_dir);
	error = PTR_ERR(old_dentry);
	if (IS_ERR(old_dentry))
		goto exit3;
	/* source must exist */
	error = -ENOENT;
	if (!old_dentry->d_inode)
		goto exit4;
	/* unless the source is a directory trailing slashes give -ENOTDIR */
	if (!S_ISDIR(old_dentry->d_inode->i_mode)) {
		error = -ENOTDIR;
		if (oldnd.last.name[oldnd.last.len])
			goto exit4;
		if (newnd.last.name[newnd.last.len])
			goto exit4;
	}
	/* source should not be ancestor of target */
	error = -EINVAL;
	if (old_dentry == trap)
		goto exit4;
	new_dentry = lookup_hash(&newnd.last, new_dir);
	error = PTR_ERR(new_dentry);
	if (IS_ERR(new_dentry))
		goto exit4;
	/* target should not be an ancestor of source */
	error = -ENOTEMPTY;
	if (new_dentry == trap)
		goto exit5;

	error = vfs_rename(old_dir->d_inode, old_dentry,
			new_dir->d_inode, new_dentry);
exit5:
	dput(new_dentry);
exit4:
	dput(old_dentry);
exit3:
	unlock_rename(new_dir, old_dir);
exit2:
	path_release(&newnd);
exit1:
	path_release(&oldnd);
exit:
	return error;
}

/* Return 1 if path already contains suffix: #number */
static inline int verfs_is_versioned (const char * path)
{
	int is_versioned = 0;
	path = strrchr (path, '#');
	if(path) {
		path++; /* Pass first '#' symbol */
		if(*path) {
			is_versioned = 1;
			while (*path) {
				is_versioned &= (*path <= '9' && *path >= '0');
				path++;
			}
		}
	}
	return is_versioned;
}

/* Find next filename available for storing new version */
static inline long verfs_find_next_file (const char * pathname, char ** buf)
{
	long next_version = 1;
	char * ver_pathname;
	struct nameidata nd;
	struct dentry * de;
	long retval = 0, error;
	
	ver_pathname = kmalloc(strlen(pathname) + sizeof(long) + 1, GFP_KERNEL);
	*buf = ver_pathname;
	
	/* Search until we find next free version number */
	do {
		sprintf(ver_pathname, "%s#%ld", pathname, next_version);
		next_version ++;

		/* Number of versioned files too large */
		if(next_version >= LONG_MAX) {
			goto exit;
		}
		error = path_lookup(ver_pathname, LOOKUP_PARENT, &nd);
		if(error) {
			retval = error;
			goto exit;
		}
		de = lookup_hash(&nd.last, nd.dentry);
		error = PTR_ERR(de);
		if (IS_ERR(de)) {
			retval = error;
			goto exit;
		}
		/* Found not existing version */
		if(!de->d_inode) {
			path_release(&nd);
			break;
		}
		path_release(&nd);
	}
	while (1);

exit:
	return retval;
}

/* version files, when unlinking */
asmlinkage long verfs_sys_unlink (const char __user * pathname)
{
	long retval = 0;
	char * ver_pathname = NULL;

	/* If already versioned - apply native unlink() syscall */
	if(verfs_is_versioned(pathname)) {
		retval = (linux_sys_unlink) (pathname);
		goto exit;
	}

	retval = verfs_find_next_file (pathname, &ver_pathname);
	if(retval) {
		goto clean_exit;
	}

	/* Move the file to the versioned filename notation: file#version */
	retval = do_rename (pathname, ver_pathname);
	
clean_exit:
	kfree(ver_pathname);
exit:
	return retval;
}

/* support open(..., O_TRUNC, ...) when versioning files */
asmlinkage long verfs_sys_open (const char __user * filename, int flags, int mode)
{
	long retval = 0;
	char * ver_pathname = NULL;
	
	if(verfs_is_versioned (filename) || !(flags & O_TRUNC)) {
		retval = (linux_sys_open) (filename, flags, mode);
		goto exit;
	}

	retval = verfs_find_next_file (filename, &ver_pathname);
	if(retval) {
		goto clean_exit;
	}

	/* Move the file to the versioned filename notation: file#version */
	retval = do_rename (filename, ver_pathname);
	if(retval) {
		goto clean_exit;
	}

	/* Proceed with native open() syscall */
	retval = (linux_sys_open) (filename, flags | O_CREAT, mode);
	
clean_exit:
	kfree(ver_pathname);
exit:
	return retval;
}

/* Initialize module */
static int __init verfs_init(void)
{
	/* hijack system call open() */
	linux_sys_open = (long (*)(const char *, int, int)) sys_call_table[__NR_open];
	sys_call_table[__NR_open] = (long) verfs_sys_open;

	/* hujack system call unlink() */
	linux_sys_unlink = (long (*)(const char *)) sys_call_table[__NR_unlink];
	sys_call_table[__NR_unlink] = (long) verfs_sys_unlink;

	linux_sys_rename = (long (*)(const char *, const char *)) sys_call_table[__NR_rename];

	printk ("VerFS version %s\n", VERFS_VERSION);
	return 0;
}

/* Cleanup module */
static void __exit verfs_exit(void)
{
	/* return old system calls */
	sys_call_table[__NR_open] = (long) linux_sys_open;
	sys_call_table[__NR_unlink] = (long) linux_sys_unlink;
}

module_init (verfs_init);
module_exit (verfs_exit);

MODULE_DESCRIPTION("VerFS - versioning of files upon remove");
MODULE_LICENSE("GPL");
