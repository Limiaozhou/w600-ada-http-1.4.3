#include "hal/pin.h"
#include "device.h"

static struct rt_device_pin _hw_pin;

static rt_size_t _pin_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size);
static rt_size_t _pin_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size);
static rt_err_t  _pin_control(rt_device_t dev, int cmd, void *args);

const static struct rt_device_ops pin_ops =
{
    RT_NULL,
    RT_NULL,
    RT_NULL,
    _pin_read,
    _pin_write,
    _pin_control
};

int rt_device_pin_register(const char *name, const struct rt_pin_ops *ops, void *user_data)
{
    _hw_pin.parent.type         = RT_Device_Class_Miscellaneous;
    _hw_pin.parent.rx_indicate  = RT_NULL;
    _hw_pin.parent.tx_complete  = RT_NULL;

    _hw_pin.parent.ops          = &pin_ops;

    _hw_pin.ops                 = ops;
    _hw_pin.parent.user_data    = user_data;

    /* register a character device */
    rt_device_register(&_hw_pin.parent, name, RT_DEVICE_FLAG_RDWR);

    return 0;
}

static rt_size_t _pin_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    struct rt_device_pin_status *status;
    struct rt_device_pin *pin = (struct rt_device_pin *)dev;

    /* check parameters */
    if(!pin)
		return 0;

    status = (struct rt_device_pin_status *) buffer;
    if (status == RT_NULL || size != sizeof(*status)) return 0;

    status->status = pin->ops->pin_read(dev, status->pin);
    return size;
}

static rt_size_t _pin_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    struct rt_device_pin_status *status;
    struct rt_device_pin *pin = (struct rt_device_pin *)dev;

    /* check parameters */
    if(!pin)
		return 0;

    status = (struct rt_device_pin_status *) buffer;
    if (status == RT_NULL || size != sizeof(*status)) return 0;

    pin->ops->pin_write(dev, (rt_base_t)status->pin, (rt_base_t)status->status);

    return size;
}

static rt_err_t  _pin_control(rt_device_t dev, int cmd, void *args)
{
    struct rt_device_pin_mode *mode;
    struct rt_device_pin *pin = (struct rt_device_pin *)dev;

    /* check parameters */
    if(!pin)
		return -RT_ERROR;

    mode = (struct rt_device_pin_mode *) args;
    if (mode == RT_NULL) return -RT_ERROR;

    pin->ops->pin_mode(dev, (rt_base_t)mode->pin, (rt_base_t)mode->mode);

    return 0;
}

/* RT-Thread Hardware PIN APIs */
void rt_pin_mode(rt_base_t pin, rt_base_t mode)
{
    if(!_hw_pin.ops)
		return;

    _hw_pin.ops->pin_mode(&_hw_pin.parent, pin, mode);
}

void rt_pin_write(rt_base_t pin, rt_base_t value)
{
    if(!_hw_pin.ops)
		return;
	
    _hw_pin.ops->pin_write(&_hw_pin.parent, pin, value);
}

int  rt_pin_read(rt_base_t pin)
{
    if(!_hw_pin.ops)
		return 0;
	
    return _hw_pin.ops->pin_read(&_hw_pin.parent, pin);
}

rt_err_t rt_pin_attach_irq(s32 pin, u32 mode,
                             void (*hdr)(void *args), void  *args)
{
    if(!_hw_pin.ops)
		return -RT_ERROR;
	
    if(_hw_pin.ops->pin_attach_irq)
    {
        return _hw_pin.ops->pin_attach_irq(&_hw_pin.parent, pin, mode, hdr, args);
    }
    return RT_ENOSYS;
}

rt_err_t rt_pin_detach_irq(s32 pin)
{
    if(!_hw_pin.ops)
		return -RT_ERROR;
	
    if(_hw_pin.ops->pin_detach_irq)
    {
        return _hw_pin.ops->pin_detach_irq(&_hw_pin.parent, pin);
    }
    return RT_ENOSYS;
}

rt_err_t rt_pin_irq_enable(rt_base_t pin, u32 enabled)
{
    if(!_hw_pin.ops)
		return -RT_ERROR;
	
    if(_hw_pin.ops->pin_irq_enable)
    {
        return _hw_pin.ops->pin_irq_enable(&_hw_pin.parent, pin, enabled);
    }
    return RT_ENOSYS;
}
