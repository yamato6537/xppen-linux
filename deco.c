#include <linux/kernel.h>       /* pr_xxx() */
#include <linux/slab.h>         /* kxalloc(), kfree() */
#include <linux/module.h>       /* MODULE_XXX(), THIS_MODULE */
#include <linux/usb/input.h>
#include <asm/unaligned.h>

#define DRIVER_NAME     "deco"
#define DRIVER_VERSION  "0.0.2"
#define DRIVER_AUTHOR   "lambda2111@gmail.com"
#define DRIVER_DESC     "XP-Pen Deco01 Tablet Driver"
#define DRIVER_LICENSE  "GPL"

#define MOD_INFO(fmt, ...) pr_info("%s: " fmt, DriverName, ##__VA_ARGS__)
#define MOD_ERR(fmt, ...) pr_err("%s: in %s(), " fmt, DriverName, __func__, ##__VA_ARGS__)

struct drvdata
{
    char phys[256];
    struct input_dev *input;
    struct urb *urb;
};

static char DriverName[] = DRIVER_NAME;

/*********************************************************************************
 * USB Device Table
 *********************************************************************************/
#define MATCH_VPI(VID, PID, IFN) \
    .match_flags= USB_DEVICE_ID_MATCH_VENDOR | \
                USB_DEVICE_ID_MATCH_PRODUCT | \
                USB_DEVICE_ID_MATCH_INT_NUMBER, \
    .idVendor = VID, \
    .idProduct = PID, \
    .bInterfaceNumber = IFN

static const struct usb_device_id devices[] =
{
    {MATCH_VPI(0x28bd, 0x0042, 1)},
    { }
};

/*********************************************************************************
 * Report handler
 *********************************************************************************/
static void report_handler(u8 *buf, struct drvdata *data)
{
    struct input_dev *input = data->input;

    switch (buf[0]) {
    case 0x06:
        input_report_key(input, BTN_0, buf[1] & 0x01);
        input_report_key(input, BTN_1, buf[1] & 0x02);
        input_report_key(input, BTN_2, buf[1] & 0x04);
        input_report_key(input, BTN_3, buf[1] & 0x08);
        input_report_key(input, BTN_4, buf[1] & 0x10);
        input_report_key(input, BTN_5, buf[1] & 0x20);
        input_report_key(input, BTN_6, buf[1] & 0x40);
        input_report_key(input, BTN_7, buf[1] & 0x80);
        input_sync(input);
        break;
    case 0x07:
        input_report_key(input, BTN_TOOL_PEN, (~buf[1]) >> 6);
        input_report_key(input, BTN_TOUCH,    buf[1] & 0x01);
        input_report_key(input, BTN_STYLUS,   buf[1] & 0x02);
        input_report_key(input, BTN_STYLUS2,  buf[1] & 0x04);
        input_report_abs(input, ABS_X, get_unaligned_le16(buf + 2)); 
        input_report_abs(input, ABS_Y, get_unaligned_le16(buf + 4));
        input_report_abs(input, ABS_PRESSURE, get_unaligned_le16(buf + 6));
        input_sync(input);
        break;
    }
}

/*********************************************************************************
 * complete for urb
 *********************************************************************************/
static void urb_complete(struct urb *urb)
{
    switch (urb->status) {
    case 0:
        report_handler(urb->transfer_buffer, urb->context);
        goto submit;
    case -ECONNRESET:
    case -ENOENT:
    case -ESHUTDOWN:
        MOD_INFO("urb shutting down with %d.\n", urb->status);
        return;
    default:
        MOD_INFO("urb status(%d) received.\n", urb->status);
        goto submit;
    }

submit:
    {
        int res = usb_submit_urb(urb, GFP_ATOMIC);
        if (res)
            MOD_INFO("usb_submit_urb() failed with %d.\n", res);
    }
}

/*********************************************************************************
 * open() and close() for input device
 *********************************************************************************/
static int input_open(struct input_dev *input)
{
    struct drvdata *data = input_get_drvdata(input); 
    int res;

    res = usb_submit_urb(data->urb, GFP_KERNEL);
    if (res) {
        MOD_ERR("usb_submit_urb() failed with %d.\n", res);
    }

    return res;
}

static void input_close(struct input_dev *input)
{
    struct drvdata *data = input_get_drvdata(input); 

    usb_kill_urb(data->urb);
}

static void setup_input_dev(struct input_dev *input, struct usb_interface *intf, char *phys)
{
    input->name = "Deco01";
    input->phys = phys;
    input->open = input_open;
    input->close = input_close;
    input->dev.parent = &intf->dev;
    usb_to_input_id(interface_to_usbdev(intf), &input->id);

    input_set_capability(input, EV_KEY, BTN_TOOL_PEN);
    input_set_capability(input, EV_KEY, BTN_TOUCH);
    input_set_capability(input, EV_KEY, BTN_STYLUS);
    input_set_capability(input, EV_KEY, BTN_STYLUS2);

    input_set_capability(input, EV_KEY, BTN_0);
    input_set_capability(input, EV_KEY, BTN_1);
    input_set_capability(input, EV_KEY, BTN_2);
    input_set_capability(input, EV_KEY, BTN_3);
    input_set_capability(input, EV_KEY, BTN_4);
    input_set_capability(input, EV_KEY, BTN_5);
    input_set_capability(input, EV_KEY, BTN_6);
    input_set_capability(input, EV_KEY, BTN_7);

    input_set_abs_params(input, ABS_X, 0, 25400, 0, 0);
    input_set_abs_params(input, ABS_Y, 0, 15875, 0, 0);
    input_set_abs_params(input, ABS_PRESSURE, 0, 8192, 0, 0);
}

/*********************************************************************************
 *  probe() and disconnect() for USB driver
 *********************************************************************************/
static int dev_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    struct usb_device *dev = interface_to_usbdev(intf);
    struct usb_endpoint_descriptor *epd = &intf->cur_altsetting->endpoint[0].desc;
    unsigned int pipe = usb_rcvintpipe(dev, epd->bEndpointAddress);    
    struct drvdata *data;
    unsigned char *buf;
    dma_addr_t dma;
    int res;

    MOD_INFO("probe\n");
    MOD_INFO("bInterfaceNumber=%d\n", intf->cur_altsetting->desc.bInterfaceNumber);
    MOD_INFO("wMaxPacketSize=%d\n", epd->wMaxPacketSize);
    MOD_INFO("bInterval=%d\n", epd->bInterval);

    /******************** driver data ********************/
    data = kmalloc(sizeof(struct drvdata), GFP_KERNEL);
    if (data == NULL) {
        MOD_ERR("kmalloc() failed.\n");
        res = -ENOMEM;
        goto failed0;
    }

    usb_set_intfdata(intf, data);

    /******************** interrupt urb ********************/
    data->urb = usb_alloc_urb(0, GFP_KERNEL);
    if (data->urb == NULL) {
        MOD_ERR("usb_alloc_urb() failed.\n");
        res = -ENOMEM;
        goto failed1;
    }

    buf = usb_alloc_coherent(dev, epd->wMaxPacketSize, GFP_KERNEL, &dma);
    if (buf == NULL) {
        MOD_ERR("usb_alloc_coherent() failed.\n");
        res = -ENOMEM;
        goto failed2;
    }

    usb_fill_int_urb(data->urb, dev, pipe, buf, epd->wMaxPacketSize, urb_complete, data, epd->bInterval);
    data->urb->transfer_dma = dma;
    data->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    /******************** input device ********************/
    data->input = devm_input_allocate_device(&intf->dev);
    if (data->input == NULL) {
        MOD_ERR("input_allocate_device(pen) failed.\n");
        res = -ENOMEM;
        goto failed3;
    }

    input_set_drvdata(data->input, data);

    usb_make_path(dev, data->phys, sizeof(data->phys));
    strlcat(data->phys, "/input0", sizeof(data->phys));

    setup_input_dev(data->input, intf, data->phys);

    res = input_register_device(data->input);    
    if (res) {
        MOD_ERR("input_register_device(pen) failed.\n");
        goto failed3;
    }

    return 0;

failed3:
    usb_free_coherent(dev, epd->wMaxPacketSize, buf, dma);
failed2:
    usb_free_urb(data->urb);
failed1:
    kfree(data);
failed0:
    usb_set_intfdata(intf, NULL);
    return res;
}

static void dev_disconnect(struct usb_interface *intf)
{
    struct drvdata *data = usb_get_intfdata(intf);

    if (data) {
        usb_kill_urb(data->urb);  
        usb_free_coherent(
            data->urb->dev,
            data->urb->transfer_buffer_length,
            data->urb->transfer_buffer,
            data->urb->transfer_dma);
        usb_free_urb(data->urb);
        kfree(data);
    }

    MOD_INFO("disconnect\n");
}

/*********************************************************************************
 * init() and exit() for module
 *********************************************************************************/
static struct usb_driver driver =
{
    .name = DriverName,
    .id_table = devices,
    .probe = dev_probe,
    .disconnect = dev_disconnect,
};

static int __init mod_init(void)
{
    int ret;

    MOD_INFO("init\n");
    MOD_INFO("Version=%s\n", DRIVER_VERSION);

    ret = usb_register(&driver);
    if (ret) {
        MOD_ERR("usb_register() failed with %d.\n", ret);
        return ret;
    }

    return 0;
}

static void __exit mod_exit(void)
{
    usb_deregister(&driver);

    MOD_INFO("exit\n");
}

/*********************************************************************************
 * module declarations
 *********************************************************************************/
module_init(mod_init);
module_exit(mod_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);
MODULE_DEVICE_TABLE(usb, devices);
