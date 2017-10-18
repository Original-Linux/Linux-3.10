/*
 * driver core demo code
 *
 * (C) 2017.10 <buddy.zhang@aliyun.com>
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/workqueu.h>

#include "device.h"

static LIST_HEAD(demo_deferred_probe_pending_list);
static LIST_HEAD(demo_deferred_active_list);
static struct workqueue_struct *demo_deferred_wq;
static atomic_t demo_deferred_trigger_count = ATOMIC_INIT(0);
static bool demo_driver_deferred_probe_enable = false;

static int demo_driver_sysfs_add(struct demo_device *dev)
{
    int ret;

    if (dev->bus)
        blocking_notifier_call_chain(&dev->bus->p->bus_notifier,
                DEMO_BUS_NOTIFY_BIND_DRIVER, dev);
    ret = sysfs_create_link(&dev->driver->p->kobj, &dev->kobj,
                kobject_name(&dev->kobj));
    if (ret == 0) {
        ret = sysfs_create_link(&dev->kobj, &dev->driver->p->kobj,
              "demo_driver");
        if (ret)
            sysfs_remove_link(&dev->driver->p->kobj,
               kobject_name(&dev->kobj));
    }
    return 0;
}

void demo_driver_deferred_probe_del(struct demo_device *dev)
{
    if (!list_empty(&dev->p->deferred_probe)) {
        printk(KERN_DEBUG "Removed from deferred list\n");
        list_del_init(&dev->p->deferred_probe);
    }
}

/* kick off re-probing deferred devices */
static void demo_driver_deferred_probe_trigger(void)
{
    if (!demo_driver_deferred_probe_enable)
        return 0;

    /*
     * A successful probe means that all the devices in the pending list
     * should be triggered to be reprobed. Move all the deferred devices
     * into the active list so they can be retried by the workqueue.
     */
    atomic_inc(&demo_deferred_trigger_count);
    list_splice_tail_init(&demo_deferred_probe_pending_list,
                          &demo_deferred_probe_active_list);
    queue_work(demo_deferred_wq, &demo_deferred_probe_work);
}

static void demo_driver_bound(struct demo_device *dev)
{
    if (klist_node_attached(&dev->p->knode_driver)) {
        printk(KERN_WARNING "%s: device %s already bound\n",
               __func__, kobject_name(&dev->kobj));
        return;
    }
    printk(KERN_INFO "driver: '%s': %s: bound to device '%s'\n", 
           demo_dev_name(dev), __func__, dev->driver->name);

    klist_add_tail(&dev->p->knode_driver, &dev->driver->p->klist_devices);

    /*
     * Make sure the demo device is no longer in one of the deferred lists
     * and kick off retrying all pending devices.
     */
    demo_driver_deferred_probe_del(dev);
    demo_driver_deferred_probe_trigger();

    if (dev->bus)
        blocking_notifier_call_chain(&dev->bus->p->bus_notifier,
             DEMO_BUS_NOTIFY_BOUND_DRIVER, dev);
}

/* bind a demo driver to one demo device */
int demo_device_bind_driver(struct demo_device *dev)
{
    int ret;

    ret = demo_driver_sysfs_add(dev);
    if (!ret)
        demo_driver_bound(dev);
    return ret;
}

/* Enable probing of deferred devices */
static int demo_deferred_probe_initcall(void)
{
    demo_deferred_wq = create_singlethread_workqueue("demo_deferwq");
    if (WARN_ON(!demo_deferred_wq))
        return -ENOMEM;

    demo_driver_deferred_probe_enable = true;
    demo_driver_deferred_probe_trigger();
    /* Sort as many dependencies as possible before exiting initcalls */
    flush_workqueue(demo_deferred_wq);
    return 0;
}
late_initcall(demo_deferred_probe_initcall);

static int __demo_device_attach(struct demo_device_driver *drv, void *data)
{
    struct demo_device *dev = data;

    if (!demo_driver_match_device(drv, dev))
        return 0;

    return demo_driver_probe_device(drv, dev);
}

/* try to attach demo device to a demo driver */
int demo_device_attach(struct demo_device *dev)
{
    int ret = 0;

    if (dev->driver) {
        if (klist_node_attached(&dev->p->knode_driver)) {
            ret = 1;
        }
        ret = demo_device_bind_driver(dev);
        if (ret == 0)
            ret = 1;
        else {
            dev->driver = NULL;
            ret = 0;
        }
    } else {
        ret = demo_bus_for_each_drv(dev->bus, NULL, dev, __demo_device_attach);
    }
    return ret;
}
