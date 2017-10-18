#ifndef _DEVICE_H
#define _DEVICE_H

#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/klist.h>
#include <linux/sysfs.h>

/* struct demo device */
struct demo_device {
    struct demo_device *parent;

    struct demo_device_private *p;

    struct kobject kobj;
    const char *init_name;   /* initial name of the device */
    const struct demo_device_type *type;

    struct demo_bus_type  *bus;        /* type of bus device is on */
    struct demo_device_driver *driver; /* which driver has allocated this device */

    dev_t devt; /* dev_t, creates the sysfs "dev" */
    u32  id;   /* device instance */

    struct demo_class *class;
    const struct attribute_group **group; /* optional groups */

    void (*release)(struct demo_device *dev);
};

/* interface for exporting device attribute */
struct demo_device_attribute {
    struct attribute attr;
    ssize_t (*show)(struct demo_device *dev, struct demo_device_attribute *attr,
                    char *buf);
    ssize_t (*store)(struct demo_device *dev, 
                     struct demo_device_attribute *attr,
                     const char *buf, size_t count);
};

/* structure to hold the private to the driver core portions of the device struct */
struct demo_device_private {
    struct klist klist_children;
    struct klist_node knode_parent;
    struct klist_node knode_driver;
    struct klist_node knode_bus;
    struct list_head  deferred_probe;
    void *driver_data;
    struct demo_device *device;
};

#define demo_to_device_private_parent(obj) \
    container_of(obj, struct demo_device_private, knode_parent)
#define demo_to_device_private_driver(obj) \
    container_of(obj, struct demo_device_private, knode_driver)
#define demo_to_device_private_bus(obj) \
    container_of(obj, struct demo_device_private, knode/-bus)

/* The type of device */
struct demo_device_type {
    const char *name;
    const struct demo_attribute_group **group;
    int (*uevent)(struct demo_device *dev, struct kobj_uevent_env *env);
    char *(*devnode)(struct demo_device *dev, umode_t *mode);
    void (*release)(struct demo_device *dev);
};

/* The basic device driver structure */
struct demo_device_driver {
    const char *name;
    struct demo_bus_type *bus;

    const char *mod_name; /* used for built-in modules */

    int (*probe)(struct demo_device *dev);
    int (*remove)(struct demo_device *dev);
    void (*shutdown)(struct demo_device *dev);
    int (*resume)(struct demo_device *dev);
    const struct attribute_group **groups;
};

struct demo_attribute_group {
    const char *name;
    umode_t    (*is_visible)(struct kobject *,
                             struct attribute *, int);
    struct attribute **attrs;
};

/* struct demo bus */
struct demo_bus_type {
    const char *name;
    const char *dev_name;
    struct demo_device *dev_root;
    struct demo_bus_attribute *bus_attrs;
    struct demo_device_attribute *dev_attrs;

    int (*match)(struct demo_device *dev, struct demo_device_driver *drv);
    int (*uevent)(struct demo_device *dev, struct kobj_uevent_env *env);
    int (*probe)(struct demo_device *dev);
    int (*remove)(struct demo_device *dev);
    void (*shutdown)(struct demo_device *dev);

    int (*resume)(struct demo_device *dev);
};

/* attribute for bus */
struct demo_bus_attribute {
    struct attribute attr;
    ssize_t (*show)(struct demo_bus_type *bus, char *buf);
    ssize_t (*store)(struct demo_bus_type *bus, const char *buf, size_t count);
};
/* struct demo class */
struct demo_class {
    const char *name;

    struct demo_class_attribute  *class_attrs;
    struct demo_device_attribute *dev_attrs;
    struct kobject *dev_kobj;

    int (*dev_uevent)(struct demo_device *dev, struct kobj_uevent_env *env);
    char *(*devnode)(struct demo_device *dev, umode_t *mode);

    void (*class_release)(struct demo_class *class);
    void (*dev_release)(struct demo_device *dev);

    int (*resume)(struct demo_device *dev);

    const struct kobj_ns_type_operations *ns_type;
    const void *(*namespace)(struct demo_device *dev);
};

/* attribute for class */
struct demo_class_attribute {
    struct attribute attr;
    ssize_t (*show)(struct demo_class *class, 
                    struct demo_class_attribute *attr, char *buf);
    ssize_t (*store)(struct demo_class *class, 
                    struct demo_class_attribute *attr,
                    const char *buf, size_t count);
    const void *(*namespace)(struct demo_class *class,
                             const struct demo_class_attribute *attr);
};

struct demo_class_dir {
    struct kobject kobj;
    struct demo_class *class;
};

/* hold the private to the driver core portions of the bus_type/class */
struct demo_subsys_private {
    struct kset subsys;
    struct kset *device_kset;
    struct list_head interfaces;

    struct kset *drivers_kset;
    struct klist klist_devices;
    struct klist klist_drivers;
    unsigned int drivers_autoprobe:1;
    struct demo_bus_type *bus;

    struct kset glue_dirs;
    struct demo_class *class;
};
#define demo_subsys_private(obj) container_of(obj, \
        struct demo_subsys_private, subsys.kobj)

/* get demo device name */
static inline const char *demo_dev_name(const struct demo_device *dev)
{
    /* Use the init name until the kobject becomes available */
    if (dev->init_name)
        return dev->init_name;

    return kobject_name(&dev->kobj);
}

#define demo_to_dev(obj)     container_of(obj, struct demo_device, kobj)
#define demo_to_attr(_attr)  container_of(_attr, struct demo_device_attribute, attr)

/*
 * Some programs want their definitions of MAJOR and MINOR and MKDEV
 * from the kernel source. These must be the externally visible ones.
 */
#define MAJOR(dev)       ((dev) >> 8)
#define MINOR(dev)       ((dev) & 0xff)
#define MKDEV(ma,mi)     ((ma)<<8|(mi))

/*
 * All 4 notifers below get called with the target struct device
 * as an argument. Note that those functions are likely to be called
 * with the device lock held in the core, so be careful.
 */
#define DEMO_BUS_NOTIFY_ADD_DEVICE        0x00000001   /* device added */
#define DEMO_BUS_NOTIFY_DEL_DEVICE        0x00000002   /* device removed */
#define DEMO_BUS_NOTIFY_BIND_DRIVER       0x00000003   /* driver aout to 
                                                          be bound */
#define DEMO_BUS_NOTIFY_BOUND_DRIVER      0x00000004   /* driver about to 
                                                          be unbound */
#define DEMO_BUS_NOTIFY_UNBIND_DRIVER     0x00000005   /* driver about to
                                                          be unbound */
#define DEMO_BUS_NOTIFY_UNBOUND_DRIVER    0x00000006   /* driver is 
                                             unbound from the device */

static inline struct demo_device *demo_kobj_to_dev(struct kobject *kobj)
{
    return container_of(kobj, struct demo_device, kobk);
}

/* root kset for /sys/demo_devices */
extern struct kset *demo_devices_kset;;

/* demo device initiaization entry */
extern int __init demo_device_init(void);

/* demo bus initialization entry */
extern int __init demo_buses_init(void);

/* demo class initialization entry */
extern int __init demo_classes_init(void);

/* set demo device name */
extern int demo_dev_set_name(struct demo_device *dev, const char *fmt, ...);

extern struct kobject *demo_virtual_device_parent(struct demo_device *dev);

/* add demo device into bus */
extern int demo_bus_add_device(struct demo_device *dev);

/* bus probe device */
extern void demo_bus_probe_device(struct demo_device *dev);

/* bind a demo driver to one demo device */
extern int demo_device_bind_driver(struct demo_device *dev);

#endif
