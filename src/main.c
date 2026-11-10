#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h> // misc dev
#include <linux/fs.h>         // file operations
#include <linux/uaccess.h>    // copy to/from user space
#include <linux/wait.h>       // waiting queue
#include <linux/sched.h>      // TASK_INTERRUMPIBLE
#include <linux/delay.h>      // udelay
#include <linux/interrupt.h>
#include <linux/gpio.h>

#include "placa_auxiliar.h"

#define DRIVER_AUTHOR "Jose Carlos D. N. & Carlos G. G."
#define DRIVER_DESC   "Device driver for the first DSO project"

static void cleanup(void)
{
    printk(KERN_NOTICE "%s: cleaning up\n", KBUILD_MODNAME);
    
    speaker_cleanup();
    leds_cleanup();

    printk(KERN_NOTICE "%s: Done.\n", KBUILD_MODNAME);
    
    return;
}

static int init(void)
{
    int res;
    
    printk(KERN_NOTICE "%s: module loading...\n", KBUILD_MODNAME);
	
	res = leds_init();
    if (res < 0) 
        return res;

    res = speaker_init();
    if (res < 0) {
        leds_cleanup();
        return res;
    }

    return 0;
}

module_init(init);
module_exit(cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
