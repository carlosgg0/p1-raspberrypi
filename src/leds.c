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


#define NUM_LEDS 6
#define NUM_MINORS (NUM_LEDS + 1)

#define GPIO_RED_LED1    9
#define GPIO_RED_LED2    10
#define GPIO_YELLOW_LED1 11
#define GPIO_YELLOW_LED2 17
#define GPIO_GREEN_LED1  22
#define GPIO_GREEN_LED2  27

#define CORRECT_GPIO(n) (512+n)

static int gpio_led[] = {	
    CORRECT_GPIO(GPIO_RED_LED1),
	CORRECT_GPIO(GPIO_RED_LED2),
	CORRECT_GPIO(GPIO_YELLOW_LED1),
	CORRECT_GPIO(GPIO_YELLOW_LED2),
	CORRECT_GPIO(GPIO_GREEN_LED1),
	CORRECT_GPIO(GPIO_GREEN_LED2) 
};

char *led_desc[] = {
    "LED ROJO 1",
    "LED ROJO 2", 
    "LED AMARILLO 1",
    "LED AMARILLO 2",
    "LED VERDE 1",
    "LED VERDE 2"
};

static char leds = '\0';

static int gpios_reserved;
static bool device_registered;

void mask_leds(int mode, char mask)
{
    switch (mode) {
        case 0: // Enciende leds de bits a 1 y apaga los de bits a 0
            leds = mask;
            break;

        case 1: // Enciende leds de bits a 1 y deja el resto igual
            leds = (char)((int)leds | (int)mask);
            break;

        case 2: // Apaga los leds de bits a 1 y deja el resto igual
            leds = (char)((int)leds & ~(int)mask);
            break;

        case 3: // NOT de todos los bits
            leds = (char)((int)leds ^ (int)mask);
            break;
        
        default:
            break;
    }
}

static ssize_t leds_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    unsigned char ch;
    int led, mode;

    if (count == 0) 
        return count;
    
    if (copy_from_user(&ch, buf, 1)) 
        return -EFAULT;

    mode = (int)ch >> 6;

    mask_leds(mode, ch & 0x3F);

    for (led = 0; led < NUM_LEDS; led++) {
        gpio_set_value(gpio_led[led], ((int)leds >> led) & 1);
    }
    
    return count;
}

static ssize_t leds_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    if (*ppos > 0 || count == 0) 
        return 0;
    
    if (copy_to_user(buf, &leds, 1)) 
        return -EFAULT;
    
    *ppos += 1;
    
    return 1;
}

static const struct file_operations leds_fops = {
    .owner      = THIS_MODULE,
    .read       = leds_read,
    .write      = leds_write,
};

// device struct
static struct miscdevice misc_dev = {
    .minor      = MISC_DYNAMIC_MINOR,
    .name       = "leds",
    .fops       = &leds_fops,
    .mode       = S_IRUGO | S_IWUGO,
};

static int dev_config(void)
{
    int ret = 0;
    
    ret = misc_register(&misc_dev);
    if (!ret)
        device_registered = true;

    return ret;
}

static int gpio_config(void)
{
    int res;

    for (int i = 0; i < NUM_LEDS; i++) {
        if (!gpio_is_valid(gpio_led[i])) 
            return -EINVAL;

        res = gpio_request_one(gpio_led[i], GPIOF_INIT_LOW, led_desc[i]);
        if (res < 0) 
            return res;

        gpios_reserved++;
	}

    return 0;
}

int leds_init(void)
{
	int res;
    
    res = gpio_config();
    if (res < 0)
        goto error;

    res = dev_config();
    if (res < 0)
        goto error;
	
    return 0;

error:
    leds_cleanup();
    return res;
}

void leds_cleanup(void)
{
    int i;
    
    if (device_registered) {
        misc_deregister(&misc_dev);
        device_registered = false;
    }

    while (gpios_reserved > 0) {
        gpios_reserved--;
        gpio_free(gpio_led[gpios_reserved]);
    }
    
    return;
}


