#ifndef PIN_H__
#define PIN_H__

#include "define.h"

#ifdef __cplusplus
extern "C" {
#endif

/* pin device and operations for RT-Thread */
struct rt_device_pin
{
    struct rt_device parent;
    const struct rt_pin_ops *ops;
};

#define PIN_LOW                 0x00
#define PIN_HIGH                0x01

#define PIN_MODE_OUTPUT         0x00
#define PIN_MODE_INPUT          0x01
#define PIN_MODE_INPUT_PULLUP   0x02
#define PIN_MODE_INPUT_PULLDOWN 0x03
#define PIN_MODE_OUTPUT_OD      0x04

#define PIN_IRQ_MODE_RISING             0x00
#define PIN_IRQ_MODE_FALLING            0x01
#define PIN_IRQ_MODE_RISING_FALLING     0x02
#define PIN_IRQ_MODE_HIGH_LEVEL         0x03
#define PIN_IRQ_MODE_LOW_LEVEL          0x04

#define PIN_IRQ_DISABLE                 0x00
#define PIN_IRQ_ENABLE                  0x01

#define PIN_IRQ_PIN_NONE                -1

struct rt_device_pin_mode
{
    u16 pin;
    u16 mode;
};
struct rt_device_pin_status
{
    u16 pin;
    u16 status;
};
struct rt_pin_irq_hdr
{
    s16        pin;
    u16       mode;
    void (*hdr)(void *args);
    void             *args;
};
struct rt_pin_ops
{
    void (*pin_mode)(struct rt_device *device, rt_base_t pin, rt_base_t mode);
    void (*pin_write)(struct rt_device *device, rt_base_t pin, rt_base_t value);
    int (*pin_read)(struct rt_device *device, rt_base_t pin);

    /* TODO: add GPIO interrupt */
    rt_err_t (*pin_attach_irq)(struct rt_device *device, s32 pin,
                      u32 mode, void (*hdr)(void *args), void *args);
    rt_err_t (*pin_detach_irq)(struct rt_device *device, s32 pin);
    rt_err_t (*pin_irq_enable)(struct rt_device *device, rt_base_t pin, u32 enabled);
};

int rt_device_pin_register(const char *name, const struct rt_pin_ops *ops, void *user_data);

void rt_pin_mode(rt_base_t pin, rt_base_t mode);
void rt_pin_write(rt_base_t pin, rt_base_t value);
int  rt_pin_read(rt_base_t pin);
rt_err_t rt_pin_attach_irq(s32 pin, u32 mode,
                             void (*hdr)(void *args), void  *args);
rt_err_t rt_pin_detach_irq(s32 pin);
rt_err_t rt_pin_irq_enable(rt_base_t pin, u32 enabled);

#ifdef __cplusplus
}
#endif

#endif
