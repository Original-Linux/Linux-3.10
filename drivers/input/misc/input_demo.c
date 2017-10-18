#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <asm/irq.h>
#include <asm/io.h>

static struct input_dev *demo_dev;

static int __init demo_init(void)
{
    int err;
    
    /* Allocate input device */
    demo_dev = input_allocate_device();
    if (!demo_dev) {
        printk(KERN_ERR "Demo: No enough memory\n");
        err = -ENOMEM;
        goto err_out1;    
    }

    /* Device information */
    demo_dev->name = "Demo_input";
    demo_dev->phys = "Demo_input/phy_path";
    demo_dev->id.vendor  = 0x0001;
    demo_dev->id.product = 0x0001;
    demo_dev->id.version = 0x0100; 

    /* Register input device */
    err = input_register_device(demo_dev);
    if (err) {
        printk(KERN_ERR "Demo: Failed to register device\n");
        goto err_out2;    
    }

    return 0;

err_out2:
    input_free_device(demo_dev);
err_out1:
    return err;
}

static void __exit demo_exit(void)
{
    input_unregister_device(demo_dev);    
}

module_init(demo_init);
module_exit(demo_exit);
