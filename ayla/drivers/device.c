#include "device.h"
#include "list.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

#define device_init     (dev->ops->init)
#define device_open     (dev->ops->open)
#define device_close    (dev->ops->close)
#define device_read     (dev->ops->read)
#define device_write    (dev->ops->write)
#define device_control  (dev->ops->control)

static rt_list_t object_list = {&object_list, &object_list};

static void rt_object_init(struct rt_object   *object,
                    u8                 type,
                    const char         *name);
static void rt_object_detach(rt_object_t object);

/**
 * This function will initialize an object and add it to object system
 * management.
 *
 * @param object the specified object to be initialized.
 * @param type the object type.
 * @param name the object name. In system, the object's name must be unique.
 */
static void rt_object_init(struct rt_object   *object,
                    u8                 type,
                    const char         *name)
{
	if(!object)
		return;

    object->type = type | RT_Object_Class_Static;

    strncpy(object->name, name, RT_NAME_MAX);

	/* insert object into information object list */
	rt_list_insert_after(&object_list, &(object->list));
}

/**
 * This function will detach a static object from object system,
 * and the memory of static object is not freed.
 *
 * @param object the specified object to be detached.
 */
static void rt_object_detach(rt_object_t object)
{
	if(!object)
		return;

    object->type = 0;

    rt_list_remove(&(object->list));
}

/**
 * This function finds a device driver by specified name.
 *
 * @param name the device driver's name
 *
 * @return the registered device driver on successful, or RT_NULL on failure.
 */
rt_device_t rt_device_find(const char *name)
{
    struct rt_object *object;
    struct rt_list_node *node;

    for (node  = object_list.next; node != &(object_list); node  = node->next)
    {
        object = rt_list_entry(node, struct rt_object, list);
		
        if (strncmp(object->name, name, RT_NAME_MAX) == 0)
            return (rt_device_t)object;
    }
	
    /* not found */
    return RT_NULL;
}

/**
 * This function registers a device driver with specified name.
 *
 * @param dev the pointer of device driver structure
 * @param name the device driver's name
 * @param flags the capabilities flag of device
 *
 * @return the error code, RT_EOK on initialization successfully.
 */
rt_err_t rt_device_register(rt_device_t dev,
                            const char *name,
                            u16 flags)
{
    if (!dev)
        return -RT_ERROR;

    if (rt_device_find(name) != RT_NULL)
        return -RT_ERROR;

    rt_object_init(&(dev->parent), RT_Object_Class_Device, name);
    dev->flag = flags;
    dev->ref_count = 0;
    dev->open_flag = 0;

    return RT_EOK;
}

/**
 * This function removes a previously registered device driver
 *
 * @param dev the pointer of device driver structure
 *
 * @return the error code, RT_EOK on successfully.
 */
rt_err_t rt_device_unregister(rt_device_t dev)
{
	if(!dev)
		return -RT_ERROR;

    rt_object_detach(&(dev->parent));

    return RT_EOK;
}

/**
 * This function creates a device object with user data size.
 *
 * @param type, the kind type of this device object.
 * @param attach_size, the size of user data.
 *
 * @return the allocated device object, or RT_NULL when failed.
 */
rt_device_t rt_device_create(int type, int attach_size)
{
    rt_device_t device;

    /* use the total size */
    attach_size += sizeof(struct rt_device);

    device = (rt_device_t)malloc((size_t)attach_size);
    if (device)
    {
        memset(device, 0x0, sizeof(struct rt_device));
        device->type = (enum rt_device_class_type)type;
    }

    return device;
}

/**
 * This function destroy the specific device object.
 *
 * @param dev, the specific device object.
 */
void rt_device_destroy(rt_device_t dev)
{
    if(!dev)
		return;

    rt_object_detach(&(dev->parent));

    /* release this device object */
    free(dev);
}

/**
 * This function initializes all registered device driver
 *
 * @return the error code, RT_EOK on successfully.
 *
 * @deprecated since 1.2.x, this function is not needed because the initialization
 *             of a device is performed when application opens it.
 */
rt_err_t rt_device_init_all(void)
{
    return RT_EOK;
}

/**
 * This function will set the reception indication callback function. This callback function
 * is invoked when this device receives data.
 *
 * @param dev the pointer of device driver structure
 * @param rx_ind the indication callback function
 *
 * @return RT_EOK
 */
rt_err_t
rt_device_set_rx_indicate(rt_device_t dev,
                          rt_err_t (*rx_ind)(rt_device_t dev, rt_size_t size))
{
	if(!dev)
		return -RT_ERROR;

    dev->rx_indicate = rx_ind;

    return RT_EOK;
}

/**
 * This function will set the indication callback function when device has
 * written data to physical hardware.
 *
 * @param dev the pointer of device driver structure
 * @param tx_done the indication callback function
 *
 * @return RT_EOK
 */
rt_err_t
rt_device_set_tx_complete(rt_device_t dev,
                          rt_err_t (*tx_done)(rt_device_t dev, void *buffer))
{
	if(!dev)
		return -RT_ERROR;

    dev->tx_complete = tx_done;

    return RT_EOK;
}

/**
 * This function will initialize the specified device
 *
 * @param dev the pointer of device driver structure
 *
 * @return the result
 */
rt_err_t rt_device_init(rt_device_t dev)
{
    rt_err_t result = RT_EOK;

    if(!dev)
		return -RT_ERROR;

    /* get device_init handler */
    if (device_init != RT_NULL)
    {
        if (!(dev->flag & RT_DEVICE_FLAG_ACTIVATED))
        {
            result = device_init(dev);
            if (result != RT_EOK)
            {
                printf("To initialize device:%s failed. The error code is %d\n",
                           dev->parent.name, (int)result);
            }
            else
            {
                dev->flag |= RT_DEVICE_FLAG_ACTIVATED;
            }
        }
    }

    return result;
}

/**
 * This function will open a device
 *
 * @param dev the pointer of device driver structure
 * @param oflag the flags for device open
 *
 * @return the result
 */
rt_err_t rt_device_open(rt_device_t dev, u16 oflag)
{
    rt_err_t result = RT_EOK;

    if(!dev)
		return -RT_ERROR;

    /* if device is not initialized, initialize it. */
    if (!(dev->flag & RT_DEVICE_FLAG_ACTIVATED))
    {
        if (device_init != RT_NULL)
        {
            result = device_init(dev);
            if (result != RT_EOK)
            {
                printf("To initialize device:%s failed. The error code is %d\n",
                           dev->parent.name, (int)result);

                return result;
            }
        }

        dev->flag |= RT_DEVICE_FLAG_ACTIVATED;
    }

    /* device is a stand alone device and opened */
    if ((dev->flag & RT_DEVICE_FLAG_STANDALONE) &&
        (dev->open_flag & RT_DEVICE_OFLAG_OPEN))
    {
        return -RT_EBUSY;
    }

    /* call device_open interface */
    if (device_open != RT_NULL)
    {
        result = device_open(dev, oflag);
    }
    else
    {
        /* set open flag */
        dev->open_flag = (oflag & RT_DEVICE_OFLAG_MASK);
    }

    /* set open flag */
    if (result == RT_EOK || result == -RT_ENOSYS)
    {
        dev->open_flag |= RT_DEVICE_OFLAG_OPEN;

        dev->ref_count++;
    }

    return result;
}

/**
 * This function will close a device
 *
 * @param dev the pointer of device driver structure
 *
 * @return the result
 */
rt_err_t rt_device_close(rt_device_t dev)
{
    rt_err_t result = RT_EOK;

    if( (!dev) || (dev->ref_count == 0) )
		return -RT_ERROR;

    dev->ref_count--;

    if (dev->ref_count != 0)
        return RT_EOK;

    /* call device_close interface */
    if (device_close != RT_NULL)
    {
        result = device_close(dev);
    }

    /* set open flag */
    if (result == RT_EOK || result == -RT_ENOSYS)
        dev->open_flag = RT_DEVICE_OFLAG_CLOSE;

    return result;
}

/**
 * This function will read some data from a device.
 *
 * @param dev the pointer of device driver structure
 * @param pos the position of reading
 * @param buffer the data buffer to save read data
 * @param size the size of buffer
 *
 * @return the actually read size on successful, otherwise negative returned.
 *
 * @note since 0.4.0, the unit of size/pos is a block for block device.
 */
rt_size_t rt_device_read(rt_device_t dev,
                         rt_off_t    pos,
                         void       *buffer,
                         rt_size_t   size)
{
    if ( (!dev) || (dev->ref_count == 0) )
        return 0;

    /* call device_read interface */
    if (device_read != RT_NULL)
    {
        return device_read(dev, pos, buffer, size);
    }

    return 0;
}

/**
 * This function will write some data to a device.
 *
 * @param dev the pointer of device driver structure
 * @param pos the position of written
 * @param buffer the data buffer to be written to device
 * @param size the size of buffer
 *
 * @return the actually written size on successful, otherwise negative returned.
 *
 * @note since 0.4.0, the unit of size/pos is a block for block device.
 */
rt_size_t rt_device_write(rt_device_t dev,
                          rt_off_t    pos,
                          const void *buffer,
                          rt_size_t   size)
{
    if ( (!dev) && (dev->ref_count == 0) )
        return 0;

    /* call device_write interface */
    if (device_write != RT_NULL)
    {
        return device_write(dev, pos, buffer, size);
    }

    return 0;
}

/**
 * This function will perform a variety of control functions on devices.
 *
 * @param dev the pointer of device driver structure
 * @param cmd the command sent to device
 * @param arg the argument of command
 *
 * @return the result
 */
rt_err_t rt_device_control(rt_device_t dev, int cmd, void *arg)
{
	if(!dev)
		return -RT_ERROR;

    /* call device_write interface */
    if (device_control != RT_NULL)
    {
        return device_control(dev, cmd, arg);
    }

    return -RT_ENOSYS;
}
