/*
    FUSE: Filesystem in Userspace
    Copyright (C) 2001  Miklos Szeredi (mszeredi@inf.bme.hu)

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include "fuse_i.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/file.h>

#define FUSE_SUPER_MAGIC 0x65735546

static void fuse_read_inode(struct inode *inode)
{
	/* No op */
}

static void fuse_clear_inode(struct inode *inode)
{
	struct fuse_conn *fc = INO_FC(inode);
	struct fuse_in in = FUSE_IN_INIT;
	struct fuse_forget_in arg;
	
	arg.version = inode->i_version;
	
	in.h.opcode = FUSE_FORGET;
	in.h.ino = inode->i_ino;
	in.argsize = sizeof(arg);
	in.arg = &arg;
	
	request_send(fc, &in, NULL);
}

static void fuse_put_super(struct super_block *sb)
{
	struct fuse_conn *fc = sb->u.generic_sbp;

	spin_lock(&fuse_lock);
	fc->sb = NULL;
	fuse_release_conn(fc);
	spin_unlock(&fuse_lock);

}

static struct super_operations fuse_super_operations = {
	read_inode:	fuse_read_inode,
	clear_inode:	fuse_clear_inode,
	put_super:	fuse_put_super,
};


static struct fuse_conn *get_conn(struct fuse_mount_data *d)
{
	struct fuse_conn *fc = NULL;
	struct file *file;
	struct inode *ino;

	if(d == NULL) {
		printk("fuse_read_super: Bad mount data\n");
		return NULL;
	}

	if(d->version != FUSE_KERNEL_VERSION) {
		printk("fuse_read_super: Bad version: %i\n", d->version);
		return NULL;
	}

	file = fget(d->fd);
	ino = NULL;
	if(file)
		ino = file->f_dentry->d_inode;
	
	if(!ino || ino->u.generic_ip != proc_fuse_dev) {
		printk("fuse_read_super: Bad file: %i\n", d->fd);
		goto out;
	}

	fc = file->private_data;

  out:
	fput(file);
	return fc;

}

static struct inode *get_root_inode(struct super_block *sb, unsigned int mode)
{
	struct fuse_attr attr;
	memset(&attr, 0, sizeof(attr));

	attr.mode = mode;
	return fuse_iget(sb, 1, &attr, 0);
}

static struct super_block *fuse_read_super(struct super_block *sb, 
					   void *data, int silent)
{	
	struct fuse_conn *fc;
	struct inode *root;
	struct fuse_mount_data *d = data;

        sb->s_blocksize = 1024;
        sb->s_blocksize_bits = 10;
        sb->s_magic = FUSE_SUPER_MAGIC;
        sb->s_op = &fuse_super_operations;

	root = get_root_inode(sb, d->rootmode);
	if(root == NULL) {
		printk("fuse_read_super: failed to get root inode\n");
		return NULL;
	}

	spin_lock(&fuse_lock);
	fc = get_conn(d);
	if(fc == NULL)
		goto err;

	if(fc->sb != NULL) {
		printk("fuse_read_super: connection already mounted\n");
		goto err;
	}

        sb->u.generic_sbp = fc;
	sb->s_root = d_alloc_root(root);
	if(!sb->s_root)
		goto err;

	fc->sb = sb;
	spin_unlock(&fuse_lock);
	
	return sb;

  err:
	spin_unlock(&fuse_lock);
	iput(root);
	return NULL;
}


static DECLARE_FSTYPE(fuse_fs_type, "fuse", fuse_read_super, 0);

int fuse_fs_init()
{
	int res;

	res = register_filesystem(&fuse_fs_type);
	if(res)
		printk("fuse: failed to register filesystem\n");

	return res;
}

void fuse_fs_cleanup()
{
	unregister_filesystem(&fuse_fs_type);
}

/* 
 * Local Variables:
 * indent-tabs-mode: t
 * c-basic-offset: 8
 * End: */

