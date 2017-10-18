/*
 * procfs interface demo code
 *
 * (C) 2017.10 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

/* proc root */
static struct proc_dir_entry *root;

/* proc file */
static struct proc_dir_entry *node;

/* initialize entry */
static __init int demo_proc_init(void)
{
    int ret;

    /* Create proc dirent on /proc/ */
    root = proc_mkdir("demo", NULL);
    if (IS_ERR(root) || !root) {
        printk(KERN_ERR "ERR: Unable to create root proc.\n");
        return -EINVAL;
    }

    node = proc_create("node", S_IRUGO | S_IWUGO, root);
    if (IS_ERR(node) || !node) {
        printk(KERN_ERR "ERR: Unable to create proc file.\n");
        node = NULL;
        ret = -EINVAL;
        goto out;
    }

    printk("proc interface initialization.\n");
    return 0;

/* error area */
out:
    remove_proc_entry("demo", root);
    return ret;
}
device_initcall(demo_proc_init);

/* exit entry */
static __exit void demo_proc_exit(void)
{
    /* Release resource */
    remove_proc_entry("node", node);
    remove_proc_entry("demo", root);
}
