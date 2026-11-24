#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/miscdevice.h> // misc dev
#include <linux/fs.h>         // file operations
#include <linux/cdev.h>       // cdev structure
#include <linux/uaccess.h>    // copy to/from user space
#include <linux/gpio.h>       // gpio_get/set_value
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/kfifo.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/wait.h>

#include "placa_auxiliar.h"

#define BUTTON1 512+2
#define BUTTON2 512+3 

#define BUTTON1_DESC "button1"
#define BUTTON2_DESC "button2"

static int value = 500;
module_param(value, int, S_IRUGO); // POR DEFECTO 500 ms
MODULE_PARM_DESC(value, "Tiempo de filtrado de rebotes en milisegundos");

static DEFINE_KFIFO(buttons_fifo, char, 64);

static DECLARE_WAIT_QUEUE_HEAD(wait_queue);

static DEFINE_SEMAPHORE(fifo_semaphore, 1);

static DEFINE_SEMAPHORE(open_semaphore, 1);


static bool device_registered;

struct button_data {
    char character;
    int gpio;
    int irq;
    bool gpio_reserved;
    bool has_last;
    unsigned long last;
};

static struct button_data button1 = {
    .character = '1', .gpio = BUTTON1, .irq = -1,
};
static struct button_data button2 = {
    .character = '2', .gpio = BUTTON2, .irq = -1,
};

static irqreturn_t irq_handler(int irq, void *dev_id)
{
    struct button_data *button = dev_id;
    unsigned long now = jiffies;

    /* Cada IRQ actualiza únicamente el filtro de su propio botón. */
    if (button->has_last &&
        time_before(now, button->last + msecs_to_jiffies(value)))
        return IRQ_HANDLED;

    button->last = now;
    button->has_last = true;

    return IRQ_WAKE_THREAD;
}

static irqreturn_t irq_thread_function(int irq, void *dev_id)
{
    int res;
    struct button_data *button = dev_id;

    down(&fifo_semaphore);

    if (kfifo_is_full(&buttons_fifo)) {
        printk(KERN_NOTICE "%s: KFIFO lleno!\n", KBUILD_MODNAME);
        up(&fifo_semaphore);
        return IRQ_HANDLED;
    }

    res = kfifo_put(&buttons_fifo, button->character);
    
    printk(KERN_NOTICE "%s: Llevas %d pulsaciones", KBUILD_MODNAME, kfifo_len(&buttons_fifo));
    
    up(&fifo_semaphore);

    if (res) 
        wake_up_interruptible(&wait_queue);

    return IRQ_HANDLED;
}

static ssize_t button_read(struct file *file, char __user *buf, 
                            size_t count, loff_t *ppos)
{
    char chars_read[64];
    unsigned int n;

    if (count == 0)
        return 0;

    for(;;) {
        if (wait_event_interruptible(wait_queue, 
                                    !kfifo_is_empty(&buttons_fifo))) 
            return -ERESTARTSYS;
    
        if (down_interruptible(&fifo_semaphore)) 
            return -ERESTARTSYS;

        if (!kfifo_is_empty(&buttons_fifo))
            break;
        
        up(&fifo_semaphore);
    }

    n = kfifo_out(&buttons_fifo, chars_read, 
                (count < sizeof(chars_read)) ? count : sizeof(chars_read));
    
    up(&fifo_semaphore);

    if (copy_to_user(buf, chars_read, n)) 
        return -EFAULT;

    return n;
}

static int button_open(struct inode *inode, struct file *filp)
{
    if (filp->f_flags & O_NONBLOCK) {
        if (down_trylock(&open_semaphore))
            return -EBUSY;
    } else {
        if (down_interruptible(&open_semaphore))
            return -ERESTARTSYS;
    }

    return 0;
}

static int button_release(struct inode *inode, struct file *filp)
{
    up(&open_semaphore);
    return 0;
}

static const struct file_operations fops = {
    .owner      = THIS_MODULE,
    .read       = button_read,
    .open       = button_open,
    .release    = button_release,
};

// device struct
static struct miscdevice miscdev = {
    .minor      = MISC_DYNAMIC_MINOR,
    .name       = "buttons",
    .fops       = &fops,
    .mode       = S_IRUGO,
};

static int dev_config(void)
{
    int ret = 0;
    
    ret = misc_register(&miscdev);
    if (!ret)
        device_registered = true;

    return ret;
}

static int configure_button(struct button_data *button, const char *description)
{
    int res, irq;

    if (!gpio_is_valid(button->gpio))
        return -EINVAL;

    res = gpio_request_one(button->gpio, GPIOF_IN, description);
    if (res < 0)
        return res;
    button->gpio_reserved = true;

    irq = gpio_to_irq(button->gpio);
    if (irq < 0)
        return irq;

    res = request_threaded_irq(irq, irq_handler, irq_thread_function,
                               IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                               description, button);
    if (res < 0)
        return res;

    button->irq = irq;
    return 0;
}

int buttons_init(void)
{
    int res;

    if (value < 0)
        return -EINVAL;

    kfifo_reset(&buttons_fifo);
    button1.has_last = false;
    button2.has_last = false;

    res = configure_button(&button1, BUTTON1_DESC);
    if (res < 0)
        goto error;

    res = configure_button(&button2, BUTTON2_DESC);
    if (res < 0)
        goto error;

    res = dev_config();
    if (res)
        goto error;

    return 0;

error:
    buttons_cleanup();
    return res;
}

static void cleanup_button(struct button_data *button)
{
    if (button->irq >= 0) {
        free_irq(button->irq, button);
        button->irq = -1;
    }

    if (button->gpio_reserved) {
        gpio_free(button->gpio);
        button->gpio_reserved = false;
    }
}

void buttons_cleanup(void)
{
    if (device_registered) {
        misc_deregister(&miscdev);
        device_registered = false;
    }

    cleanup_button(&button2);
    cleanup_button(&button1);
}
