#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>    /* wait */
#include <fcntl.h>     // Unix-like system calls to open and close
#include <string.h>

#include "myutils.h"

struct sockaddr_in serv_addr,cli_addr;

#define PORT "1234"            // string format
#define LOCALHOST "127.0.0.1"  // string format
#define SIMULATIONS 100




int main(int argc, char* argv[]) {
  int sockfd, newsockfd; // Creamos 2 punteros a sockets
  socklen_t clilen;
  sockfd = socket(AF_INET, SOCK_STREAM,0); // Accedemos al socket
  printf("Socket created\n");
  fill_addr(NULL, PORT, &serv_addr); // Establecemos la dirección
  printf("About to bind\n");
  bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); // Hacemos el bind
  listen(sockfd,5); // Escuchamos a la espera de solicitudes
  clilen = sizeof(cli_addr);
  printf("Listen done\n");
  newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr,&clilen); // Aceptamos la solicitud de un ckiente
  printf("Accepted\n");
  int n = rand()%100; // Generamos un número aleatorio
  write(newsockfd, &n, 4); // y lo escribimos en el socket

  close(sockfd); // Cerramos ambos sockets
  close(newsockfd);

  return 0;
}