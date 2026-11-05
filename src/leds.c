#include "placa_auxiliar.h"

static enum LED_INDEX {
    RED_LED1,
    RED_LED2,
    YELLOW_LED1,
    YELLOW_LED2,
    GREEN_LED1,
    GREEN_LED2
};

static char leds = '0';

int LED_PINS[] = {
    512 + 9,    // rojos
    512 + 10,   
    512 + 11,   // amarillos
    512 + 17,
    512 + 22,   // verdes
    512 + 27
};

char *LED_DESCRIPTIONS[] = {
    "LED ROJO 1",
    "LED ROJO 2", 
    "LED AMARILLO 1",
    "LED AMARILLO 2",
    "LED VERDE 1",
    "LED VERDE 2"
}

int leds_init(void)
{
    // TODO: ARREGLAR ESTA FUNCIÓN
    int res;

    for (int led_index = 0; led_index < 6; led_index++) {
        if ((res = gpio_is_valid(LED_PINS[led_index])) < 0) {
            printk(KERN_ERR "%s: Invalid GPIO %d\n", KBUILD_MODNAME, LED_PINS[led_index]);
            return res;
        }
        if ((res = gpio_request(LED_PINS[led_index], LED_DESCRIPTIONS[led_index])) < 0) {
            printk(KERN_ERR "%s: GPIO request faiure: %s\n", KBUILD_MODNAME, LED_DESCRIPTIONS[led_index]);
            return res;
        }
        if ((irq_BUTTON1 = gpio_to_irq(LED_PINS[led_index])) < 0) {
            printk(KERN_ERR "%s: GPIO to IRQ mapping faiure %s\n", KBUILD_MODNAME, LED_DESCRIPTIONS[led_index]);
            return irq_BUTTON1;
        }
        
        printk(KERN_NOTICE "%s: Mapped int %d for button1 in gpio %d\n", KBUILD_MODNAME, irq_BUTTON1, GPIO_BUTTON1);
        
        if ((res = request_irq(irq_BUTTON1,
                        (irq_handler_t) r_irq_handler,
                        IRQF_TRIGGER_FALLING,
                        GPIO_BUTTON1_DESC,
                        GPIO_BUTTON1_DEVICE_DESC)) < 0) {
            printk(KERN_ERR "%s: Irq Request failure\n", KBUILD_MODNAME);
            return res;
        }
    }

    return 0;
}


void leds_cleanup(void)
{

}