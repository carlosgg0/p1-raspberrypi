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

#include "placa_auxiliar.h"

#define BUTTON1 512+2
#define BUTTON2 512+3 

#define BUTTON1_DESC "Button 1"
#define BUTTON2_DESC "Button 2"

static short irq_button1 = 0;
static short irq_button2 = 0;

static char value = '';

#define MAX_GAP_IN_JIFFIES 20
module_param(500, int, S_IRUGO); // POR DEFECTO 500 ms


static DECLARE_WAIT_QUEUE_HEAD(wait_queue);

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
    short local_value;

    return IRQ_HANDLED;
}

static ssize_t b_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    if (*ppos > 0) {
        return count;
    }
    
    spin_lock_irq(&lock); // disable irqs on that CPU and then grabs the lock
    while (should_block) // check lock condition (could be complex)
    {   // we are blocked!
        spin_unlock_irq(&lock);   // release the spin_lock, irq may grab it
        printk( KERN_INFO "%s: (read) start waiting\n", KBUILD_MODNAME);
        if (wait_event_interruptible(wait_queue, !should_block)) {
            return -ERESTARTSYS;
        }  
        spin_lock_irq(&lock);     // grab the spinlock to re-check safely the condition
    }
    printk(KERN_INFO "%s: (read) end of waiting\n", KBUILD_MODNAME);
    should_block = 1; // event consumed, so the next process should block
    spin_unlock_irq(&lock);

    if (copy_to_user(buf, "OK", len)) return -EFAULT;

    return len;
}

static const struct file_operations b_fops = {
    .owner      = THIS_MODULE,
    .read       = b_read,
};

// device struct
static struct miscdevice b_miscdev = {
    .minor      = MISC_DYNAMIC_MINOR,
    .name       = "locking",
    .fops       = &b_fops,
    .mode       = S_IRUGO | S_IWUGO,
};

int configure_button(int gpio_num, char *gpio_desc, short *irq_button) 
{
    int res;

    if ((res = gpio_is_valid(gpio_num)) < 0) {
        return res;
    }

    if ((res = gpio_request(gpio_num, gpio_desc)) < 0) {
        return res;
    }

    if ((*irq_button = gpio_to_irq(gpio_num)) < 0) {
        return *irq_button;
    }
    
    if ((res = request_threaded_irq(*irq_button,
                    (irq_handler_t) irq_handler,
                    (irq_handler_t) irq_thread_function,
                    IRQF_TRIGGER_FALLING,
                    gpio_desc,
                    gpio_desc)) < 0) {
        return res;
    }

    return 0;
}

int buttons_init(void)
{
    int res;

    if ((res = configure_button(BUTTON1, BUTTON1_DESC, &irq_button1))) {
        buttons_cleanup();
        return res;
    }

    if ((res = configure_button(BUTTON2, BUTTON2_DESC, &irq_button2))) {
        buttons_cleanup();
        return res;
    }

    return 0;
}

void buttons_cleanup(void) 
{
    if (irq_button1 > 0) {
        free_irq(irq_button1, BUTTON1_DESC);
        gpio_free(BUTTON1);
    }

    if (irq_button2 > 0) {
        free_irq(irq_button2, BUTTON2_DESC);
        gpio_free(BUTTON2);
    }

    return;
}
