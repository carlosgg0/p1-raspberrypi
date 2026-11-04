Proyecto de Diseño de Sistemas Operativos

Estructura inicial
------------------
Makefile                  Compilación del módulo mediante Kbuild.
src/main.c                Inicialización y liberación del módulo y recursos comunes.
src/leds.c                Gestión de /dev/leds.
src/speaker.c             Gestión de /dev/speaker.
src/buttons.c             Gestión de /dev/buttons, interrupciones y sincronización.
include/placa_auxiliar.h   Declaraciones compartidas entre los ficheros del módulo.

Los ficheros C y la cabecera están vacíos, pendientes de implementación.

Compilación
-----------
Se necesitan make, un compilador y las cabeceras del kernel de destino.
En la Raspberry Pi, con las cabeceras correspondientes al kernel en ejecución:

    make

Si las cabeceras están en otra ubicación:

    make KERNEL_SRC=/ruta/al/kernel/build

La compilación copia el Makefile y los fuentes a build/ y ejecuta Kbuild allí.
Los ficheros generados quedan dentro de ese directorio, ignorado por Git.
Una vez implementado, se generará el módulo build/placa_auxiliar.ko.
La estructura vacía todavía no constituye un módulo compilable.
Para compilar desde otra arquitectura también habrá que configurar ARCH y
CROSS_COMPILE para el kernel y la cadena de compilación de destino.

Limpieza de los ficheros generados:

    make clean

La limpieza elimina build/ completo y no necesita las cabeceras del kernel.
