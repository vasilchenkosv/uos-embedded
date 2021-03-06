#include "runtime/lib.h"
#include "kernel/uos.h"
#include <timer/timer.h>
#include <mem/mem.h>
#include <usb/usbdev.h>
#include <usb/usb_struct.h>
#include <usb/usb_const.h>
#include <pic32/usbdevhal.h>

ARRAY (task_space, 0x400);

usbdev_t usb;
mem_pool_t pool;
timer_t timer;
mutex_t usb_lock;

static const usb_dev_desc_t device_descriptor = {
    sizeof (usb_dev_desc_t),
    USB_DESC_TYPE_DEVICE,
    0x0200,
    0,
    0,
    0,
    USBDEV_EP0_MAX_SIZE,
    0x04d8,
    0x003e,
    0x0100,
    1,
    2,
    0,
    1
};

typedef struct _this_conf_desc_t
{
    usb_conf_desc_t     conf;
    usb_iface_desc_t    iface;
} this_conf_desc_t;

static const this_conf_desc_t config_descriptor = {
    // Configuration descriptor
    {
        sizeof (usb_conf_desc_t),
        USB_DESC_TYPE_CONFIG,
        sizeof (this_conf_desc_t),
        1,
        1,
        0,
        USB_CONF_ATTR_REQUIRED | USB_CONF_ATTR_SELF_PWR,
        50,
    },
    
    // Interface descriptor
    {
        sizeof (usb_iface_desc_t),
        USB_DESC_TYPE_IFACE,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    }
};

// Language code string descriptor
const struct {
    unsigned char bLength;
    unsigned char bDscType;
    uint16_t string [1];
} sd000 = {
    sizeof(sd000),
    USB_DESC_TYPE_STRING,
    { 0x0409 }
};

// Manufacturer string descriptor
const struct {
    unsigned char bLength;
    unsigned char bDscType;
    uint16_t string [12];
} sd001 = {
    sizeof(sd001),
    USB_DESC_TYPE_STRING,
{   'U','O','S','-','e','m','b','e','d','d','e','d'
}};

// Product string descriptor
const struct {
    unsigned char bLength;
    unsigned char bDscType;
    uint16_t string [10];
} sd002 = {
    sizeof(sd002),
    USB_DESC_TYPE_STRING,
{   'U','S','B','-','D','e','v','i','c','e'
}};

const void *usb_strings[] = {
    (const void *) &sd000,
    (const void *) &sd001,
    (const void *) &sd002
};

static void task (void *arg)
{
    debug_printf ("Free memory: %d bytes\n", mem_available (&pool));
    for (;;) {
        timer_delay (&timer, 1000);
        //mdelay (1000);
        debug_printf ("U1IR = %02X\n", U1IR);
    }
}

void uos_init (void)
{
    extern unsigned __bss_end[], _estack[];

    debug_printf ("=============== TEST-USBDEV ================\n");	
	
    mem_init (&pool, (unsigned) __bss_end, (unsigned) _estack - 256);
	
    timer_init (&timer, KHZ, 100);

    usbdev_init (&usb, &pool, &device_descriptor);
    usbdev_add_config_desc (&usb, &config_descriptor);
    usbdev_set_string_table (&usb, usb_strings);
    
    pic32_usbdev_init (&usb, 10, &pool, &usb_lock);
    task_create (task, "task", "task", 1, task_space, sizeof (task_space));
}
