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

#include "placa_auxiliar.h"

#define BUTTON1 512+2
#define BUTTON2 512+3 

#define BUTTON1_DESC "button1"
#define BUTTON2_DESC "button2"

#define MAX_GAP_IN_JIFFIES 20

static int value = 500;
module_param(value, int, S_IRUGO); // POR DEFECTO 500 ms

static DEFINE_KFIFO(buttons_fifo, char, 64);

static DECLARE_WAIT_QUEUE_HEAD(wait_queue);

static DEFINE_SEMAPHORE(fifo_semaphore, 1);

static DEFINE_SEMAPHORE(open_semaphore, 1);

static int irq_button1 = 0;
static int irq_button2 = 0;

static bool device_registered;

struct button_data {
    char character;
};

static struct button_data button1 = { .character = '1' };
static struct button_data button2 = { .character = '2' };

static irqreturn_t irq_handler(int irq, void *dev_id)
{
    static unsigned long long last = 0;
    unsigned long long now = get_jiffies_64();
    
    if (now - last < MAX_GAP_IN_JIFFIES) {
        return IRQ_HANDLED;
    }

    last = now;

    return IRQ_WAKE_THREAD;
}

static irqreturn_t irq_thread_function(int irq, void *dev_id)
{
    int res;
    struct button_data *button = dev_id;

    down(&fifo_semaphore);
    res = kfifo_put(&buttons_fifo, button->character);
    up(&fifo_semaphore);

    if (res) 
        wake_up_interruptible(&wait_queue);

    return IRQ_HANDLED;
}

static ssize_t button_read(struct file *file, char __user *buf, 
                            size_t count, loff_t *ppos)
{
    if (count == 0) 
        return 0;
    
    char chars_read[64];
    unsigned int n;

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
    .mode       = S_IRUGO | S_IWUGO,
};

static int dev_config(void)
{
    int ret = 0;
    
    ret = misc_register(&miscdev);
    if (!ret)
        device_registered = true;

    return ret;
}

int configure_button(int gpio_num, char *gpio_desc, int *irq_button, struct button_data *button) 
{
    int res;

    if ((res = gpio_is_valid(gpio_num))) {
        return res;
    }

    if ((res = gpio_request(gpio_num, gpio_desc)) < 0) {
        return res;
    }

    if ((*irq_button = gpio_to_irq(gpio_num)) < 0) {
        return *irq_button;
    }
    
    if ((res = request_threaded_irq(*irq_button,
                                    irq_handler,
                                    irq_thread_function,
                                    IRQF_TRIGGER_FALLING,
                                    gpio_desc,
                                    button)) < 0) {
        return res;
    }

    return 0;
}

int buttons_init(void)
{
    int res;

    if ((res = configure_button(BUTTON1, 
                                BUTTON1_DESC, 
                                &irq_button1,
                                &button1))) {
        goto error;
    }

    if ((res = configure_button(BUTTON2, 
                                BUTTON2_DESC, 
                                &irq_button2,
                                &button2))) {
        goto error;
    }

    res = dev_config();
    if (res)
        goto error;

    return 0;

error:
    buttons_cleanup();
    return res;
}

void buttons_cleanup(void) 
{
    if (device_registered) {
        misc_deregister(&miscdev);
        device_registered = false;
    }

    if (irq_button1 > 0) {
        free_irq(irq_button1, &button1);
        gpio_free(BUTTON1);
    }

    if (irq_button2 > 0) {
        free_irq(irq_button2, &button2);
        gpio_free(BUTTON2);
    }

    return;
}
