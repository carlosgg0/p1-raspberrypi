#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEVICE "/dev/buttons"

static void esperar_enter(void)
{
    int c;

    while ((c = getchar()) != '\n' && c != EOF) {
    }
}

int main(int argc, char *argv[])
{
    int fd;
    int flags = O_RDONLY;
    int result = EXIT_SUCCESS;
    const char *mode;

    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc != 2) {
        fprintf(stderr,
                "Uso: %s ocupar|abrir|noblock|leer|fifo\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    mode = argv[1];

    if (strcmp(mode, "ocupar") != 0 &&
        strcmp(mode, "abrir") != 0 &&
        strcmp(mode, "noblock") != 0 &&
        strcmp(mode, "leer") != 0 &&
        strcmp(mode, "fifo") != 0) {
        fprintf(stderr, "Modo desconocido: %s\n", mode);
        return EXIT_FAILURE;
    }

    if (strcmp(mode, "noblock") == 0) {
        flags |= O_NONBLOCK;
    }

    printf("PID %ld: intentando abrir %s\n",
           (long)getpid(), DEVICE);

    fd = open(DEVICE, flags);

    if (fd == -1) {
        if (errno == EBUSY) {
            fprintf(stderr, "open(): EBUSY, dispositivo ocupado\n");
        } else {
            perror("open");
        }

        return EXIT_FAILURE;
    }

    printf("PID %ld: apertura completada\n", (long)getpid());

    if (strcmp(mode, "ocupar") == 0 ||
        strcmp(mode, "abrir") == 0) {
        printf("Dispositivo reservado. Pulsa ENTER para cerrarlo.\n");
        esperar_enter();
    } else if (strcmp(mode, "noblock") == 0) {
        printf("Apertura con O_NONBLOCK correcta: estaba libre.\n");
    } else if (strcmp(mode, "leer") == 0) {
        for (int i = 0; i < 5; i++) {
            char key;
            ssize_t res;

            printf("Esperando pulsacion %d de 5...\n", i + 1);
            res = read(fd, &key, 1);

            if (res == -1) {
                perror("read");
                result = EXIT_FAILURE;
                break;
            }

            if (res != 1 || (key != '1' && key != '2')) {
                fprintf(stderr, "Lectura inesperada\n");
                result = EXIT_FAILURE;
                break;
            }

            printf("Recibido: '%c'\n", key);
        }
    } else if (strcmp(mode, "fifo") == 0) {
        const char expected[] = {'1', '2', '1'};

        printf("Sin pulsaciones anteriores pendientes, pulsa 1, 2, 1.\n");
        printf("Deja tiempo entre pulsaciones para el antirrebotes.\n");
        printf("Despues pulsa ENTER aqui para empezar las lecturas.\n");
        esperar_enter();

        for (int i = 0; i < 3; i++) {
            char key;
            ssize_t res = read(fd, &key, 1);

            if (res == -1) {
                perror("read");
                result = EXIT_FAILURE;
                break;
            }

            if (res != 1) {
                fprintf(stderr, "Numero de bytes inesperado\n");
                result = EXIT_FAILURE;
                break;
            }

            printf("Esperado '%c', recibido '%c': %s\n",
                   expected[i], key,
                   key == expected[i] ? "OK" : "FALLO");

            if (key != expected[i]) {
                result = EXIT_FAILURE;
            }
        }
    }

    if (close(fd) == -1) {
        perror("close");
        result = EXIT_FAILURE;
    }

    printf("Prueba terminada\n");
    return result;
}