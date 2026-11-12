#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void){
    int fd = open("/dev/speaker", O_WRONLY);
    int result = 0;

    if(fd == -1){
        perror("open /dev/speaker");
        return 1;
    }

    for(int cycle = 0; cycle < 440; cycle++){
        if(write(fd, "1", 1) != 1){
            fprintf(stderr, "No se pudo activar el altavoz\n");
            result = 1;
            break;
        }
        usleep(1136);

        if(write(fd, "0", 1) != 1){
            fprintf(stderr, "No se pudo desactivar el altavoz\n");
            result = 1;
            break;
        }
        usleep(1136);
    }

    if(write(fd, "0", 1) != 1){
        fprintf(stderr, "No se pudo dejar el altavoz apagado\n");
        result = 1;
    }

    close(fd);
    return result;
    
}