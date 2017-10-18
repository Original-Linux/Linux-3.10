/*
 * hlist demo code
 *
 * (C) 2017.09.10 buddy.zhang@aliyun.com
 *
 */
#include <linux/init.h>
#include <linux/kernel.h>

static __init int hlist_demo_init(void)
{
    printk("hlist demo init.\n");
    return 0;
}
device_initcall(hlist_demo_init);
