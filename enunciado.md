# Proyecto de driver para la placa auxiliar Raspberry Pi
## Requisitos de finalización

Realizar un módulo de kernel, que funcione como driver para la placa auxiliar de E/S de la Raspberry

El driver debe crear tres devices diferentes, con las siguientes características:

## leds

El device llamado **leds**, debe permitir modificar y consultar qué leds están encendidos o apagados a través de `/dev/leds`.

La lectura retornará un byte con los 6 bits menos significativos representando el estado de los leds (los dos bits más significativos a cero). Para la escritura de los leds se utilizará un byte. Los 6 bits menos significativos serán los valores a escribir en los LEDs de la placa y deben interpretarse, dependiendo de los valores de los dos bits más significativos del byte escrito, de la siguiente forma:

- Si el bit 7 y 6 están a cero, los bits de 5 a 0 serán los valores que deben tomar los LEDS: 1 encender y 0 apagar.
- Si el bit 6 está a 1 y el 7 a 0, sólo se tomarán en cuenta los bits a 1 (del 5 a 0) para poner los LEDs correspondientes a 1, el resto quedarán con el valor anterior.
- Si en bit 7 está a 1 y el 6 a 0, sólo se tomarán en cuenta los bits a 1 (del 5 a 0) para poner los LEDs correspondientes a 0, el resto quedarán con el valor anterior.
- Si el bit 6 y 7 están ambos a 1, sólo se tomarán en cuenta los bits a 1 (del 5 a 0) para cambiar el valor (NOT) de los LEDs correspondientes, el resto quedará con el valor anterior.

## speaker

Este device será solo de escritura y permitirá activar y desactivar la salida conectada al altavoz piezoeléctrico. El usuario será responsable de, escribiendo en este device (`/dev/speaker`),  generar ondas que produzcan sonidos. Un byte = '0' escrito en el device, pondrá a 0 la salida conectada al altavoz. Un byte != '0', pondrá a 1 la salida del device.

## buttons

Este device será de solo lectura y permitirá leer las pulsaciones de los dos botones como si se tratase de un teclado. Para ello, el driver tendrá que manejar un buffer interno donde se almacenen las pulsaciones de teclas, recuerda utilizar las estructuras de datos estándar del kernel de Linux (p.ej. FIFO). Y cuando un usuario lea del device `/dev/buttons` servirle dichas pulsaciones de forma que si se pulsó el botón 1, se lea el carácter ASCII que representa al número uno '1' y si se pulsó el botón dos, se lea '2'.

Deben usarse interrupciones para controlar la pulsación de teclas. Utilizar una técnica de deferred job (tasklet, timer, threaded_irq, workqueue) para una implementación eficiente de las interrupciones. Adoptar una solución software para filtrar a los rebotes de los pulsadores (una pulsación puede generar muchas interrupciones en pocos milisegundos y estós "rebotes" se deben ignorar), ofreciendo como parámetro del módulo driver la cantidad de milisegundos que se usará para filtrar los rebotes, estableciendo un valor por defecto adecuado.

Si cuando el usuario realiza una lectura del device no hay pulsaciones en el buffer, la lectura debe ser bloqueada en espera de una nueva pulsación. Se despertará la lectura del device cuando se reciba una pulsación de botón y se pueda servir la lectura al usuario. Para que la llamada a la syscall read del device buttons sea bloqueante utilizar una cola de espera. Este es el interfaz necesario para gestionar el bloqueo de las lecturas:

```
DECLARE_WAIT_QUEUE_HEAD()    // declara una cola de espera en el kernel
wait_event_interruptible()   // el proceso actual se bloquea en una cola de espera según una condición, normalmente en la función read() del dispositivo, cuando hayq ue esperar que se complete la operación de E/S
wake_up_interruptible()      // desbloquea a los procesos que esperan en una cola de espera, normalmente desde un tasklet arrancado después de la interrupción que nos informa de que ya se puede continuar la operación de E/S
```

Las funciones que bloquean a un proceso de forma interruptible, pueden ser interrumpidas por una señal recibida por el proceso. Esto permite que un proceso bloqueado responda a señales (`CTRL+C` por ejemplo). Para controlar esta situación hay que comprobar si estas funciones que se bloquean devuelven cero, si no lo hacen hay que devolver el error `-ERESTARTSYS` . Algo como:

`if (wait_event_interruptible( ... ) ) return -ERESTARTSYS;`

Esto provocará que el sistema reintente la ejecución de la función donde se produzca este error después del tratamiento de la señal del proceso interrumpido durante su bloqueo.

Referencia del interfaz de E/S bloqueante: https://bootlin.com/doc/books/ldd3.pdf chapter 6, section 2

Se puede ver un ejemplo de uso en el template LDT.

### Uso en exclusividad del device buttons

Hay que asegurarse de que el dispositivo buttons se usa en exclusividad, esto es, una vez que un proceso ha hecho `open()` sobre el dispositivo con exito, los siguientes procesos que intenten acceder al dispositivo llamando a `open()`, esta función no puede retornar con éxito. Para controlar la apertura única del dispositivo de lectura de los botones habrá que utilizar las funciones `open()` y `release()` del dispositivo para señalar cuando se abre y se cierra el fichero.

Durante la apertura del dispositivo con `open()` se comprobará si el flag `O_NONBLOCK` está o no activo. Si el flag `O_NONBLOCK` está activo y el dispositivo está en uso se retornará el error de dispositivo ocupado (EBUSY), si no está activo, el proceso quedará en espera de que se cierre para continuar correctamente.

`i(filp->f_flags & O_NONBLOCK) {...} else {...}`

Para quedar en espera se puede usar un mutex o un semáforo de kernel (https://bootlin.com/doc/books/ldd3.pdf, Linux Device Driver 3rd ed., chapter 5, section 3). Aquí os dejo los principales elementos necesarios para usar semáforos en el kernel que nos será de utilidad siempre que estemos trabajando en el contexto de un proceso (que se puede bloquear):

```C
#include <linux/semaphore.h>  // librería
DEFINE_SEMAPHORE(semaforo);  // declaración de un semáforo abierto (valor=1)

open(...)
{
  ...
  if (down_interruptible(&semaforo)) return -ERESTARTSYS; // capturamos el semaforo en caso de que el flag
                                                          // indique open bloqueante
  ...
release(...)
{
  ...
  up(&semaforo);   // liberamos el semáforo en caso de que el flag indique open bloqueante
  ...
}
```