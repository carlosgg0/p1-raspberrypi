Proyecto de Device Driver 

Alumnos: José Carlos Durán Nuño y Carlos García Guzmán

Estructura inicial
------------------
Makefile
src/main.c                  Inicialización y limpieza del módulo
src/leds.c                  Gestión de /dev/leds
src/speaker.c               Gestión de /dev/speaker
src/buttons.c               Gestión de /dev/buttons
include/placa_auxiliar.h    Declaraciones de las funciones de inicialización de cada device

Compilación:

$ make

Esta genera un directorio build/ que contiene el fichero placa_auxiliar.ko


Instalar el módulo:

$ sudo insmod build/placa_auxiliar.ko value=500 

El parámetro value hace referencia al tiempo en milisegundos que debe transcurrir
entre dos pulsaciones de un mismo botón (para filtrar los rebotes).


Eliminarlo:

$ sudo rmmod placa_auxiliar
