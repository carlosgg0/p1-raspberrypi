#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/miscdevice.h> // misc dev
#include <linux/fs.h>         // file operations
#include <linux/cdev.h>       // cdev structure
#include <linux/uaccess.h>    // copy to/from user space
#include <linux/gpio.h>       // gpio_get/set_value

#include "placa_auxiliar.h"

#define BUTTON1 512+2
#define BUTTON2 512+3 

#define BUTTON1_DESC "Button 1"
#define BUTTON2_DESC "Button 2"

static short irq_button1 = 0;
static short irq_button2 = 0;

static irqreturn_t irq_handler(int irq, void *dev_id)
{
    
    return IRQ_WAKE_THREAD;
}

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