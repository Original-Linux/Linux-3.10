/*
 * kobject demo code
 *
 * (C) 2017.10 <buddy.zhang@aliyun.com> 
 */
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/list.h>

/* demo device */
struct demo_device {
    struct demo_device *parent;
    struct kobject kobj;
    struct demo_private *p;

    const char *init_name; /* initial name of the device */

    /* device resource list */
    struct list_head devres_head;

    void (*release)(struct demo_device *dev);

    void demo_bus_type *bus;
};

/* demo attribute */
struct demo_attribute {
    struct attribute attr;
    ssize_t (*show)(struct demo_device *dev, struct demo_attribute *attr,
                    char *buf);
    ssize_t (*store)(struct demo_device *dev, struct demo_attribute *attr,
                     const char *buf, size_t count);
};

/* demo device private */
struct demo_private {
    struct klist klist_children;
    struct klist_node knode_parent;
    struct klist_node knode_driver;
    struct klist_node knode_bus;
    struct list_head deferred_probe;
    void *demo_data;
    struct demo_device *device;
};

#define to_device_private_parent(obj) \
    container_of(obj, struct demo_device_private, knode_parent)
#define to_device_private_driver(obj) \
    container_of(obj, struct demo_device_private, knode_driver)
#define to_device_private_bus(obj) \
    container_of(obj, struct demo_device_private, knode_bus)

/* The bus type of the device */
struct demo_bus_type {
    const char *name;
    const char *dev_name;
    struct demo_device *dev_root;
    struct demo_bus_attribute    *bus_attrs;
    struct demo_device_attribute *dev_attrs;
};

/* demo device resource management */
typedef void (*demo_dr_release_t)(struct demo_device *dev, void *res);
typedef int (*demo_dr_match_t)(struct demo_device *dev, void *res, 
                          void *match_data);

/* demo device resource node */
struct demo_devres_node {
    struct list_head entry;
    demo_dr_release_t release;
};

/* demo device resource */
struct demo_devres {
    struct demo_devres_node node;
    /* -- 3 pointers */
    unsigned long long data[]; /* guarantee ull alignment */
};

struct demo_devres_group {
    struct demo_devres_node node[2];
    void *id;
    int color;
    /* -- 8 pointers */
};

struct demo_class_attribute {
    struct attribute attr;
    ssize_t (*show)(struct demo_class *class, 
                    struct demo_class_attribute *attr, char *buf);
    ssize_t (*store)(struct demo_class *class,
                    struct demo_class_attribute *attr,
                    const char *buf, size_t count);
    const void *(*namespace)(struct demo_class *class,
                             const struct class_attribute *attr);
};

/* demo device class */
struct demo_class {
    const char *name;

    struct demo_class_attribute *class_attrs;
    struct demo_attribute *dev_attrs;
    struct kobject *dev_kobj;

    const void *(*namespace)(struct demo_device *dev);
};

struct demo_class_interface {
    struct list_head node;
    struct demo_class *class;

    int (*add_dev)(struct demo_device *, struct demo_class_interface *);
    void (*remove_dev)(struct demo_device *, struct demo_class_interface *);
};

static struct kobject root;

/* /sys/demo */
static struct kset *demo_kset;

#define to_dev_attr(_attr) container_of(_attr, struct demo_attribute, attr)

/* cover kobject to demo device */
static inline struct demo_device *kobj_to_dev(struct kobject *kobj)
{
    return container_of(kobj, struct demo_device, kobj);
}

static inline const char *demo_dev_name(const struct demo_device *dev)
{
    /* Use the init name until the kobject becomes available */
    if (dev->init_name)
        return dev->init_name;

    return kobject_name(&dev->kobj);
}

/*
 * Release functions for demo devres group. These callbacks are used only
 * for identification.
 */
static void demo_group_open_release(struct demo_device *dev, void *res)
{
    /* noop */
}

static void demo_group_close_release(struct demo_device *dev, void *res)
{
    /* noop */
}

static struct demo_devres_group *demo_node_to_group(
              struct demo_devres_node *node)
{
    if (node->release == &demo_group_open_release)
        return container_of(node, struct demo_devres_group, node[0]);
    if (node->release == &demo_group_close_release)
        return container_of(node, struct demo_devres_group, node[1]);
    return NULL;
}

static int demo_remove_nodes(struct demo_device *dev,
           struct list_head *first, struct list_head *end,
           struct list_head *todo)
{
    int cnt = 0, nr_groups = 0;
    struct list_head *cur;

    /* First pass - move normal devres entries to @todo and clear
     * devres_group colors. 
     */
    cur = first;
    while (cur != end) {
        struct demo_devres_node *node;
        struct demo_devres_group *grp;

        node = list_entry(cur, struct demo_devres_node, entry);
        cur = cur->next;

        grp = demo_node_to_group(node);
        if (grp) {
            /* clear color of group markers in the first pass */
            grp->color = 0;
            nr_groups++;
        } else {
            /* regular devres entry */
            if (&node->entry == first)
                first = first->next;
            list_move_tail(&node->entry, todo);
            cnt++;
        }
    }

    if (!nr_groups)
        return cnt;

    /* Second pass - Scan groups and color them. A group gets
     * color value of two iff the group is wholly contained in
     * [cur, end]. That is, for a closed group, both opening and
     * closing markers should be in the range, while just the 
     * opening marker is enough for an open group.
     */
    cur = first;
    while (cur != end) {
        struct demo_devres_node *node;
        struct demo_devres_group *grp;

        node = list_entry(cur, struct demo_devres_node, entry);
        cur = cur->next;

        grp = demo_node_to_group(node);
        BUG_ON(!grp || list_empty(&grp->node[0].entry));

        grp->color++;
        if (list_empty(&grp->node[1].entry))
            grp->color++;

        BUG_ON(grp->color <= 0 || grp->color > 2);
        if (grp->color == 2) {
            /* No need to update cur or end. The removed
             * nodes are always before both.
             */
            list_move_tail(&grp->node[0].entry, todo);
            list_del_init(&grp->node[1].entry);
        }
    }
    return cnt;
}

/* release device resource */
static int demo_release_nodes(struct demo_device *dev, 
           struct list_head *first, struct list_head *end, unsigned long flags)
{
    LIST_HEAD(todo);
    int cnt;
    struct demo_devres *dr, *tmp;

    cnt = demo_remove_nodes(dev, first, end, &todo);
    
    /* Release. Note that both demo_devres and demo_devres_group are
     * handled as devres in the following loop. This is safe.
     */
    list_for_each_entry_safe_reverse(dr, tmp, &todo, node.entry) {
        dr->node.release(dev, dr->data);
        kfree(dr);
    }
    return cnt;
}

/* Release all managed resources */
int demo_devres_release_all(struct demo_device *dev)
{
    unsigned long flags;

    /* Looks like an uninitialized device structure */
    if (WARN_ON(dev->devres_head.next == NULL))
        return -ENODEV;

    return demo_release_nodes(dev, dev->devres_head.next, 
                              &dev->devres_head, flags);
}

/* free demo device structure. */
static void demo_release(struct kobject *kobj)
{
    struct demo_device *dev = kobj_to_dev(kobj);
    struct demo_private *p = dev->p;

    demo_devres_release_all(dev);

    if (dev->release)
        dev->release(dev);
    else
        WARN(1, KERN_ERR "Demo device '%s' does not have a release() "
                "function, it is broken and must be fixed.\n",
                demo_dev_name(dev));
    kfree(p);
}

static const void *demo_namespace(struct kobject *kobj)
{
    struct demo_device *dev = kobj_to_dev(kobj);
    const void *ns = NULL;

    return ns;
}

/* demo device show */
static ssize_t demo_attr_show(struct kobject *kobj, struct attribute *attr,
               char *buf)
{
    struct demo_attribute *demo_attr = to_dev_attr(attr);
    struct demo_device *dev = kobj_to_dev(kobj);
    ssize_t ret = -EIO;

    if (demo_attr->show)
        return ret = demo_attr->show(dev, demo_attr, buf);
    return ret;
}

/* demo device store */
static ssize_t demo_attr_store(struct kobject *kobj, struct attribute *attr,
                               const char *buf, size_t count)
{
    struct demo_attribute *demo_attr = to_dev_attr(attr);
    struct demo_device *dev = kobj_to_dev(kobj);
    ssize_t ret = -EIO;

    if (demo_attr->store)
        ret = demo_attr->store(dev, demo_attr, buf, count);
    return ret;
}

/* attribute for sysfs */
static const struct sysfs_ops demo_sysfs_ops = {
    .show  = demo_attr_show,
    .store = demo_attr_store,
};

/* ktype for /sys/devices */
static struct kobj_type demo_ktype = {
    .release     = demo_release,
    .sysfs_ops   = &demo_sysfs_ops,
    .namespace   = demo_namespace,
}; 

/* kset uevent filter */
static int demo_uevent_filter(struct kset *kset, struct kobject *kobj)
{
    struct kobj_type *ktype = get_ktype(kobj);

    if (ktype == &demo_ktype) {
        struct demo_device *dev = kobj_to_dev(kobj);
        /* no bus and class */
        return 0;
    }
    return 0;
}

/* kset uevent name */
static const char *demo_uevent_name(struct kset *kset, struct kobject *kobj)
{
    struct demo_device *dev = kobj_to_dev(kobj);
    /* no bus or class */
    return NULL;
}

static int demo_uevent(struct kset *kset, struct kobject *kobj,
                       struct kobj_uevent_env *env)
{
    struct demo_device *dev = kobj_to_dev(kobj);
    int retval = 0;

    /* add demo device node proiperities present */
    return 0;
}

static const struct kset_uevent_ops demo_uevent_ops = {
    .filter    = demo_uevent_filter,
    .name      = demo_uevent_name,
    .uevent    = demo_uevent,
};

/* init demo device structure */
void demo_device_initialize(struct demo_device *dev)
{
    dev->kobj.kset = demo_kset;
    kobject_init(&dev->kobj, &demo_ktype);
}

struct kobject *demo_virtual_device_parent(struct demo_device *dev)
{
    static struct kobject *virtual_dir = NULL;

    if (!vitual_dir)
        virtual_dir = kobject_create_and_add("demo_virtual",
                      &demo_kset->kobj);
    return virtual_dir;
}

static struct kobject *demo_class_dir_create_and_add(struct demo_class *class,
              struct kobject *parent_kobj)
{
    struct demo_class_dir *dir;
    int retval;

    dir = kzalloc(sizeof(*dir), GFP_KERNEL);
    if (!dir)
        return NULL;

    dir->class = clas;
    kobject_init(&dir->kobj, &demo_class_dir_ktype);
    dir->kobj.kset = &class->p->glue_dirs;
}

/* increament reference count for demo device */
struct demo_device *demo_get_device(struct demo_device *dev)
{
    return dev ? kobj_to_dev(kobject_get(&dev->kobj)) : NULL;
}

static struct kobject *demo_get_device_parent(struct demo_device *dev,
                       struct demo_device *parent)
{
    if (dev->class) {
        struct kobject *kobj = NULL;
        struct kobject *parent_kobj;
        struct kobject *k;

        /*
         * If we have no parent, we live in "virtual".
         * Class-devices with a non class-device as parent, live
         * in a "glue" direntory to prevent namespace collisions.
         */
        if (parent == NULL)
            parent_kobj = demo_virtual_device_parent(dev);
        else if (parent->class && !dev->class->ns_type)
            return &parent->kobj;
        else
            parent_kobj = &parent->kobj;

        list_for_each_entry(k, &dev->class->p->glue_dirs.list, entry)
            if (k->parent == parent_kobj) {
                kobj = kobject_get(k);
                break;
            }
        if (kobj)
            return kobj;
        /* or create a new class-directory at the parent device */
        k = class_dir_create_and_add(dev->class, parent_kobj);
        /* do not emit an uevent for this simple "glue" directory */
        return k;
    }
    /* subsystem can specify a default root dirent for their devices */
    if (!parent && dev->bus && dev->bus->dev_root)
        return &dev->bus->dev_root->kobj;

    if (parent)
        return &parent->kobj;
    return NULL;
}

/* decrement reference count */
void demo_put_device(struct demo_device *dev)
{
    /* might_sleep(); */
    if (dev)
        kobject_put(&dev->kobj);
}

static void klist_children_get(struct klist_node *n)
{
    struct demo_device_private *p = to_device_private_parent(n);
    struct demo_device *dev = p->device;

    demo_get_device(dev);
}

static void klist_children_put(struct klist_node *n)
{
    struct demo_device_private *p = to_device_private_parent(n);
    struct demo_device *dev = p->device;

    demo_put_device(dev);
}

/* initialize demo device private data */
int demo_device_private_init(struct demo_device *dev)
{
    dev->p = kzalloc(sizeof(*dev->p), GFP_KERNEL);
    if (dev->p)
        return -ENOMEM;
    dev->p->device = dev;
    klist_init(&dev->p->klist_children, klist_children_get,
               klist_children_put);
    INIT_LIST_HEAD(&dev->p->deferred_probe);
    return 0;
}

/* set a demo device name */
int demo_dev_set_name(struct demo_device *dev, const char *fmt, ...)
{
    va_list vargs;
    int err;

    va_start(vargs, fmt);
    err = kobject_set_name_vargs(&dev->kobj, fmt, vargs);
    va_end(vargs);
    return err;
}

/* Add device to device hierarchy. */
int demo_device_add(struct demo_device *dev)
{
    struct demo_device *parent = NULL;
    struct kobject *kobj;
    struct demo_class_interface *class_intf;
    int error = -EINVAL;

    dev = demo_get_device(dev);
    if (!dev)
        goto done;

    if (!dev->p) {
        error = demo_device_private_init(dev);
        if (error)
            goto done;
    } 

    /*
     * for statically allocated devices, which should all be converted
     * some day, we need to initialize the name. We prevent reading back
     * the name, and force the use of dev_name().
     */
    if (dev->init_name) {
        demo_dev_set_name(dev, "%s", dev->init_name);
        dev->init_name = NULL;
    }

    /* subsystem can specify simple device enumeration */
    if (!demo_dev_name(dev) && dev->bus && dev->bus->dev_name)
        demo_dev_set_name(dev, "%s%u", dev->bus->dev_name, dev->id);

    if (!demo_dev_name(dev)) {
        error = -EINVAL;
        goto name_error;
    }

    printk(KERN_INFO "demo device '%s':%s\n", demo_dev_name(dev), __func__);

    parent = demo_get_device(dev->parent);
    kobj = demo_get_device_parent(dev, parent);
}

/* register a demo device into system */
int demo_device_register(struct demo_device *dev)
{
    demo_device_initialize(dev);
    return demo_device_add(dev);
}

/* initiailization entry */
static __init int demo_kobject_init(void)
{
    /* initialize /sys/demo kset */
    demo_kset = kset_create_and_add("demo", &demo_uevent_ops, NULL);

    if (!demo_kset) {
        printk(KERN_ERR "Unable to create kset.\n");
        return -ENOMEM;
    }

    printk("Demo initialize all\n");
    return 0;
}
device_initcall(demo_kobject_init);

/* exit entry */
static __exit void demo_kobject_exit(void)
{
}
