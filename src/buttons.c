#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/miscdevice.h> // misc dev
#include <linux/fs.h>         // file operations
#include <linux/cdev.h>       // cdev structure
#include <linux/uaccess.h>    // copy to/from user space
#include <linux/gpio.h>       // gpio_get/set_value
<<<<<<< HEAD
#include <linux/interrupt.h>

#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
=======
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/kfifo.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
>>>>>>> main

#include "placa_auxiliar.h"

#define BUTTON1 (512+2)
#define BUTTON2 (512+3)

#define BUTTON1_DESC "button1"
#define BUTTON2_DESC "button2"

<<<<<<< HEAD
static int irq_button1 = 0;
static int irq_button2 = 0;

#define BUTTONS_FIFO_SIZE 64

static DEFINE_KFIFO(buttons_fifo, char, BUTTONS_FIFO_SIZE);
static DEFINE_SPINLOCK(buttons_fifo_lock);
static DECLARE_WAIT_QUEUE_HEAD(buttons_wait_queue);

static unsigned int debounce_ms = 500;
module_param(debounce_ms, uint, 0444);
MODULE_PARM_DESC(debounce_ms, "Tiempo de antirrebotes en milisegundos");

static DEFINE_MUTEX(buttons_read_lock);

static struct semaphore buttons_open_sem;

static void buttons_push_event(char key){
    unsigned int inserted;

    inserted = kfifo_in_spinlocked(&buttons_fifo, &key, 1, &buttons_fifo_lock);

    if (inserted > 0) {
        wake_up_interruptible(&buttons_wait_queue);
    }
}
=======
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
>>>>>>> main

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
    int gpio_num;
    char key;

    if (dev_id == &irq_button1) {
        gpio_num = BUTTON1;
        key = '1';
    } else if (dev_id == &irq_button2) {
        gpio_num = BUTTON2;
        key = '2';
    } else {
        return IRQ_NONE;
    }

    if (debounce_ms > 0) {
        msleep(debounce_ms);
    }

    if (gpio_get_value_cansleep(gpio_num) == 0) {
        buttons_push_event(key);
    }

    return IRQ_HANDLED;
}

static bool buttons_have_events(void)
{
    unsigned long flags;
    bool available;

    spin_lock_irqsave(&buttons_fifo_lock, flags);
    available = !kfifo_is_empty(&buttons_fifo);
    spin_unlock_irqrestore(&buttons_fifo_lock, flags);

    return available;
}

static int buttons_open(struct inode *inode, struct file *file)
{
    if (file->f_flags & O_NONBLOCK) {
        if (down_trylock(&buttons_open_sem)) {
            return -EBUSY;
        }
    } else {
        if (down_interruptible(&buttons_open_sem)) {
            return -ERESTARTSYS;
        }
    }

    return 0;
}

static int buttons_release(struct inode *inode, struct file *file)
{
    up(&buttons_open_sem);
    return 0;
}

static ssize_t buttons_read(struct file *file, char __user *buf,
                            size_t count, loff_t *ppos)
{
    unsigned long flags;
    unsigned int copied;
    char key;
    int res;
    struct button_data *button = dev_id;

<<<<<<< HEAD
    if (count == 0) {
        return 0;
    }

    if (mutex_lock_interruptible(&buttons_read_lock)) {
        return -ERESTARTSYS;
    }

    res = wait_event_interruptible(buttons_wait_queue,
                                   buttons_have_events());
    if (res) {
        mutex_unlock(&buttons_read_lock);
        return -ERESTARTSYS;
    }

    spin_lock_irqsave(&buttons_fifo_lock, flags);
    copied = kfifo_out_peek(&buttons_fifo, &key, 1);
    spin_unlock_irqrestore(&buttons_fifo_lock, flags);

    if (copied == 0) {
        mutex_unlock(&buttons_read_lock);
        return -EIO;
    }

    if (copy_to_user(buf, &key, 1)) {
        mutex_unlock(&buttons_read_lock);
        return -EFAULT;
    }

    spin_lock_irqsave(&buttons_fifo_lock, flags);
    kfifo_skip(&buttons_fifo);
    spin_unlock_irqrestore(&buttons_fifo_lock, flags);

    mutex_unlock(&buttons_read_lock);
    return 1;
}

int configure_button(int gpio_num, const char *gpio_desc, int *irq_button) 
{
    int res;
    int irq;

    if(!gpio_is_valid(gpio_num)){
        return -EINVAL;
    }

    if ((res = gpio_request_one(gpio_num, GPIOF_IN, gpio_desc)) < 0) {
        return res;
    }

    if ((irq = gpio_to_irq(gpio_num)) < 0) {
        gpio_free(gpio_num);
        return irq;
    }
    
    if ((res = request_threaded_irq(irq,
                    (irq_handler_t) irq_handler,
                    (irq_handler_t) irq_thread_function,
                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,,
                    gpio_desc,
                    irq_button)) < 0) {
        gpio_free(gpio_num);
        return res;
    }

    *irq_button = irq;
=======
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
>>>>>>> main
    return 0;
}

int buttons_init(void)
{
    int res;
    sema_init(&buttons_open_sem, 1);

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
<<<<<<< HEAD
    if (irq_button1 > 0) {
        free_irq(irq_button1, &irq_button1);
        gpio_free(BUTTON1);
        irq_button1 = 0;
    }

    if (irq_button2 > 0) {
        free_irq(irq_button2, &irq_button2);
        gpio_free(BUTTON2);
        irq_button2 = 0;
=======
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
>>>>>>> main
    }

    cleanup_button(&button2);
    cleanup_button(&button1);
}
