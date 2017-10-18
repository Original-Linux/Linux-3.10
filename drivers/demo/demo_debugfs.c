/*
 * debugfs demo code.
 *
 * (C) 2017.09.06 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/seq_file.h>
#include <linux/debugfs.h>

static struct dentry *debugfs_root;

/* seq operation: start */
static void *demo_seq_start(struct seq_file *sf, loff_t *pos)
{
    return NULL + (*pos == 0);
}

/* seq operation: next */
static void *demo_seq_next(struct seq_file *sf, void *v, loff_t *pos)
{
    ++*pos;
    return NULL;
}

/* seq operation: stop */
static void demo_seq_stop(struct seq_file *fs, void *v)
{
}

/* seq operation: show */
static int demo_seq_show(struct seq_file *sf, void *m)
{
    printk("dfsdfsdaf\n");
    seq_puts(sf, "Seq show start");
    seq_putc(sf, 'c');

    return 0;
}

/* seq operations */
static struct seq_operations demo_seq_fops = {
    .start    = demo_seq_start,
    .next     = demo_seq_next,
    .stop     = demo_seq_stop,
    .show     = demo_seq_show, 
};


/* open operation */
static int debugfs_file_open(struct inode *inode, struct file *filp)
{
    /* open debugfs file with seq_file */
    seq_open(filp, &demo_seq_fops);
    return 0;
}

/* relase operation */
static int debugfs_file_release(struct inode *inode, struct file *filp)
{
    /* release debug file */
    return 0;
}

/* read operation */
static ssize_t debugfs_file_read(struct file *filp, char __user *buf, 
                                 size_t count, loff_t *loff) 
{
    /* read operation */
    return count;
}

/* write operation */
static ssize_t debugfs_file_write(struct file *filp, const char __user *buf, 
                                  size_t count, loff_t *loff)
{
    /* write to debugfs file */
    return count;
}

/* debugfs file operation */
static const struct file_operations debugfs_file_ops = {
    .open        = debugfs_file_open,
    .read        = debugfs_file_read,
    .write       = debugfs_file_write,
    .release     = debugfs_file_release,
};

/* Initialize entry */
static __init int demo_debugfs_init(void)
{
    int ret;

    /* create debugfs root directory. */
    debugfs_root = debugfs_create_dir("demo", NULL);
    if (IS_ERR(debugfs_root) || !debugfs_root) {
        printk(KERN_ERR "ERR: faild to create debugfs directory.\n");
        debugfs_root = NULL;
        return -EINVAL;
    }

    /* Create debugfs file */
    if (!debugfs_create_file("files", S_IFREG | S_IRUGO,
                        debugfs_root, NULL, &debugfs_file_ops)) {
        printk(KERN_ERR "Unable to create file on debugfs.\n");
        ret = -EINVAL;
        goto out;
    }

    printk("Debugfs demo code init.\n");
    return 0;
out:
    debugfs_remove_recursive(debugfs_root);
    debugfs_root = NULL;
    return ret;
}
device_initcall(demo_debugfs_init);

/* Exit entry */
static __exit void demo_debugfs_exit(void)
{
    /* release debugfs file */
    debugfs_remove_recursive(debugfs_root);
}
