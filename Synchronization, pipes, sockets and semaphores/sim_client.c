

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>    /* wait */
#include <fcntl.h>     // Unix-like system calls to open and close
#include <string.h>

#include "myutils.h"

struct sockaddr_in serv_addr;

#define PORT "1234"            // string format
#define LOCALHOST "127.0.0.1"  // string format
#define SIMULATIONS 100



int main(int argc, char* argv[]) {
  int sockfd;
  for(int i = 0; i < SIMULATIONS; i++){
    if(argc > 1) {
      printf("The %s command does not accept params as %s\n", argv[0], argv[1]);
      return -1;
    }
    sockfd = socket(AF_INET, SOCK_STREAM,0); // accedemos al socket
    
    // Client Connect Code
    fill_addr(LOCALHOST,PORT,&serv_addr); // Establecemos la dirección
    int retcon = connect(sockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr)); // Nos conectamos al socket
  
    
    printf("Connected! now waiting integer seed from socket\n");
    
    int num;
    // read integer from the socket  
    read(sockfd,&num,4); // Leemos la seed del socket
  
    // close socket
    close(sockfd); // Cerramos el socket
  
    printf("The received seed is: %d. Starting computation...\n", num);
    srand(num);
    for(int i=0;i<600;i++) {
        num = rand() % 1000;    
        usleep(5000+num);
    }
    printf("Ended. The result of the simulation is: %d\n", num);
  }
  
  return 0;
}

