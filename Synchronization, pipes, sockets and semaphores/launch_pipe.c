


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>    /* wait */
#include <fcntl.h>     // Unix-like system calls to open and close
#include <string.h>


#define SIMULATIONS 100

int fd[2]; // Inicializamos el array de la pipe
pipe(fd); // La syscall pipe es la encargada de crear e inicializar la pipe

int main(void) {
  
  int pid,i;

  for(i=0;i<SIMULATIONS;i++) {
      pid = fork();
      if(pid==0) {
        close(fd[1]); // Se cierra el canal de escritura de la pipe
        dup2(fd[0],0); // Se pasa el canal de lectura de la pipe al standard input, el teclado
        char* myargvs[] = {"./sim", NULL};
        execvp("./sim",myargvs);
        _exit(0);
      }
  }
  for(int i = 0; i < SIMULATIONS; i++){
    close(fd[0]); // Se cierra el canal de lectura de la pipe
    int seed = rand(); // Se crea el número aleatorio
    write(fd[1],&seed, sizeof(seed)); // Se escribe el número aleatorio en la pipe
  }

  while(wait(NULL)>0);

}
