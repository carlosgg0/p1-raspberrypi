#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>

#include "placa_auxiliar.h"

static int speaker_gpio = -1;
module_param(speaker_gpio, int, 0444);
MODULE_PARM_DESC(speaker_gpio, "Numero GPIO del altavoz");

static ssize_t speaker_write(struct file* file, const char __user *buf, size_t count, loff_t *ppos){
    char value;
    
    if(count == 0)
        return 0;
    
    if(copy_from_user(&value, buf, 1))
        return -EFAULT;

    gpio_set_value(speaker_gpio, value != '0');

    return 1;
}

static const struct file_operations speaker_fops = {
    .owner = THIS_MODULE,
    .write = speaker_write,
};

static struct miscdevice speaker_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "speaker",
    .fops = &speaker_fops,
    .mode = 0222,
};

int speaker_init(void){
    int ret;

    if(!gpio_is_valid(speaker_gpio))
        return -EINVAL;
    
    ret = gpio_request_one(speaker_gpio, GPIOF_OUT_INIT_LOW, "placa_auxiliar_speaker");

    if(ret)
        return ret;

    ret = misc_register(&speaker_device);
    if(ret){
        gpio_free(speaker_gpio);
        return ret;
    }

    return 0;
}

void speaker_cleanup(void){
    misc_deregister(&speaker_device);
    gpio_set_value(speaker_gpio, 0);
    gpio_free(speaker_gpio);
}