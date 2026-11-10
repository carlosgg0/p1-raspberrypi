#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void){
    int fd = open("/dev/speaker", O_WRONLY);

    if(fd == -1){
        perror("open /dev/speaker");
        return 1;
    }

    if(write(fd, "0", 1) != 1){
        fprintf(stderr, "No se pudo escribir un byte en /dev/speaker\n");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
    
}