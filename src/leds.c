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

static char leds = '\0';


char *led_desc[] = {
    "LED ROJO 1",
    "LED ROJO 2", 
    "LED AMARILLO 1",
    "LED AMARILLO 2",
    "LED VERDE 1",
    "LED VERDE 2"
};

// device file operations
static ssize_t leds_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    char ch;
    int minor = iminor(file->f_path.dentry->d_inode);
    int led;

    if (*ppos > 0) return count;
    if (copy_from_user(&ch, buf, 1)) return -EFAULT;
    if (minor > NUM_LEDS) return -ENOSYS;
    if (minor == 0) {
        for (led = 0; led < NUM_LEDS; led++) {
            gpio_set_value(gpio_led[led], (int)ch & 1);
        }
        printk(KERN_INFO "%s: set all leds to %d\n", KBUILD_MODNAME, (int)ch & 1);
    } else {
        printk(KERN_WARNING "%s: invalid minor number %d\n", KBUILD_MODNAME, minor);
    }
    *ppos += 1;
    return 1;
}

static ssize_t leds_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    int ch = 0;
    int minor = iminor(file->f_path.dentry->d_inode);
    int led;

    if (*ppos > 0) return 0;
    if (minor > NUM_LEDS) return -ENOSYS;
    if (minor == 0) {
        printk(KERN_WARNING "%s: invalid minor number %d\n", KBUILD_MODNAME, minor);
    } else {
        led = minor - 1;
        ch = gpio_get_value(gpio_led[led]);
        printk(KERN_INFO "%s: get led %s (%#x)\n", KBUILD_MODNAME, led_desc[led], ch);
    }
    ch += '0';
    if (copy_to_user(buf, &ch, 1)) return -EFAULT;
    *ppos += 1;
    return 1;
}

static const struct file_operations leds_fops = {
    .owner      = THIS_MODULE,
    .read       = leds_read,
    .write      = leds_write,
};

// device struct
static struct miscdevice miscdev = {
    .minor      = MISC_DYNAMIC_MINOR,
    .name       = "leds",
    .fops       = &leds_fops,
    .mode       = S_IRUGO | S_IWUGO,
};

static int gpio_config(void)
{
    int i, res;

    for(i = 0; i < NUM_LEDS; i++) {
        if ((res = gpio_is_valid(gpio_led[i])) < 0) {
            printk(KERN_ERR "%s:   Invalid GPIO %d\n", KBUILD_MODNAME, gpio_led[i]);
            return res;
        }
        if ((res = gpio_request_one(gpio_led[i], GPIOF_INIT_LOW, led_desc[i])) < 0) {
            printk(KERN_ERR "%s:   GPIO request failure: led %s GPIO %d\n",
					KBUILD_MODNAME, led_desc[i], gpio_led[i]);
            return res;
        }
        if ((res = gpio_direction_output(gpio_led[i], 0)) < 0) {
            printk(KERN_ERR "%s:   GPIO set direction output failure: led %s GPIO %d\n",
					KBUILD_MODNAME, led_desc[i], gpio_led[i]);
            return res;
        }
	}

    return 0;
}

static int r_dev_config(void)
{
    int ret = 0;
    
    ret = misc_register(&miscdev);
    if (ret < 0) 
		printk(KERN_ERR "%s: misc_register failed\n", KBUILD_MODNAME);
    else 
		printk(KERN_NOTICE "%s: misc_register OK... b_miscdev.minor=%d\n", KBUILD_MODNAME, miscdev.minor);
    
    return ret;
}

int leds_init(void)
{
	int res;
	
    printk(KERN_NOTICE "%s: module loading\n", KBUILD_MODNAME);
    
    printk(KERN_NOTICE "%s:  devices config\n", KBUILD_MODNAME);
    if ((res = r_dev_config())) {
        printk(KERN_ERR "%s:  failed\n", KBUILD_MODNAME);
        return res;
    }
    
    printk(KERN_NOTICE "%s:  ok\n", KBUILD_MODNAME);
	
    printk(KERN_NOTICE "%s:  GPIO config\n", KBUILD_MODNAME);
    if((res = gpio_config())) {
        printk(KERN_ERR "%s:  failed\n", KBUILD_MODNAME);
        leds_cleanup();
        return res;
    }

    printk(KERN_NOTICE "%s:  ok\n", KBUILD_MODNAME);
	
    return 0;
}

void leds_cleanup(void)
{
    int i;
    
    printk(KERN_NOTICE "%s: cleaning up ", KBUILD_MODNAME);
    
    for(i = 0; i < NUM_LEDS; i++) {
        gpio_free(gpio_led[i]);
    }
    
    printk(KERN_NOTICE "%s: DONE\n", KBUILD_MODNAME);
    
    return;
}


