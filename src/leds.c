#include "placa_auxiliar.h"

#define NUM_LEDS 6

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
}

int leds_init(void)
{
    // TODO: ARREGLAR ESTA FUNCIÓN
    int res;

    for (int i = 0; i < NUM_LEDS; i++) {
        if ((res = gpio_is_valid(gpio_led[i])) < 0) {
            printk(KERN_ERR "%s: Invalid GPIO %d\n", KBUILD_MODNAME, gpio_led[i]);
            return res;
        }
        if ((res = gpio_request(gpio_led[i], led_desc[i])) < 0) {
            printk(KERN_ERR "%s: GPIO request faiure: %s\n", KBUILD_MODNAME, led_desc[i]);
            return res;
        }
    }

    return 0;
}


void leds_cleanup(void)
{

}