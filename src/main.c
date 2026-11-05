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

#define DRIVER_AUTHOR "DAC-UMA"
#define DRIVER_DESC   "Device driver with interrupt handler for berryclip"

static void cleanup(void)
{
    printk(KERN_NOTICE "%s: cleaning up\n", KBUILD_MODNAME);

    // USAR LAS CORRESPONDIENTES FUNCIONES DE LIMPIEZA
    
    printk(KERN_NOTICE "%s: Done.\n", KBUILD_MODNAME);
    
    return;
}

static int init(void)
{
    printk(KERN_NOTICE "%s: module loading...\n", KBUILD_MODNAME);

    return 0;
}

module_init(init);
module_exit(cleanup);

// Module licensing & description
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);