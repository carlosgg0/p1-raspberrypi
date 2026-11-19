#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/miscdevice.h> // misc dev
#include <linux/fs.h>         // file operations
#include <linux/cdev.h>       // cdev structure
#include <linux/uaccess.h>    // copy to/from user space
#include <linux/gpio.h>       // gpio_get/set_value
#include <linux/interrupt.h>

#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>

#include "placa_auxiliar.h"

#define BUTTON1 (512+2)
#define BUTTON2 (512+3)

#define BUTTON1_DESC "Button 1"
#define BUTTON2_DESC "Button 2"

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

static irqreturn_t irq_handler(int irq, void *dev_id)
{
    
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
    return 0;
}

int buttons_init(void)
{
    int res;
    sema_init(&buttons_open_sem, 1);

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
        free_irq(irq_button1, &irq_button1);
        gpio_free(BUTTON1);
        irq_button1 = 0;
    }

    if (irq_button2 > 0) {
        free_irq(irq_button2, &irq_button2);
        gpio_free(BUTTON2);
        irq_button2 = 0;
    }

    return;
}