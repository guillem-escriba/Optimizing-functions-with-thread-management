#include <stdio.h>
#include <string.h>
#include <unistd.h>    // Unix-like system calls read and write
#include <fcntl.h>     // Unix-like system calls to open and close

#include "myutils.h"

#define WIDTH  1024
#define HEIGHT 1024
#define KLEN      7    // works with 3 or 7
#define KSIZE    49    // KLEN * KLEN

#define MAX(a, b) (a < b ? b : a)
#define MIN(a, b) (a < b ? a : b)

unsigned char* pixels; // a pointer to the original image (needs malloc)
unsigned char* target; // a pointer to the target of convolution (needs malloc)

bool all_ended = false; // booleano que sirve para intercambiar la imagen



typedef struct kernel_struct {
  int values[KSIZE];
  int sum;
} kernel_type;

typedef struct barrier_struct {
  int missing;
  pthread_mutex_t lock;
  pthread_cond_t cond;
} barrier_type;

void b_create(barrier_type* barrier, int n){ // Inicialización de la barrera
  barrier->missing = n;
  pthread_mutex_init(&barrier->lock,NULL);
  pthread_cond_init(&barrier->cond,NULL);
}

void b_wait_all(barrier_type* barrier){
  pthread_mutex_lock(&barrier->lock); // bloquea la barrera para que no pase ningún thread
  barrier->missing--; // Descuenta uno por cada thread que entre
  //printf("I've finished, now waiting for the others.\n");
  while(barrier->missing > 0){ // Mientras falten threads por entrar seguirán esperando
    pthread_cond_wait(&barrier->cond,&barrier->lock); // Cuando llegue el último thread se liberaran
  }
  //printf("I'm the last one we can all go now\n");
  all_ended = true; // Activa el booleano para que un thread cambie la imagen 
  pthread_cond_broadcast(&barrier->cond); // Despierta a los threads que se han quedado esperando
  pthread_mutex_unlock(&barrier->lock); //Desbloquea el lock
}

void b_reset(barrier_type* barrier, int n){
  barrier->missing = n; // Reinicia el contador de la barrera
}

kernel_type filter; // the 3x3 kernel used to convolve the image
barrier_type barrier_conv; // Barrier que espera a la convolución de la imagen
barrier_type barrier_image; // Barrier que espera a que se cambie la imagen

static unsigned char tga[18];

// read a 1024x1024 image
void read_tga(char* fname) {
  int fd = open(fname, O_RDONLY);
  read(fd, tga, 18);
  read(fd, pixels, WIDTH * HEIGHT);
  close(fd);
}

// write a 1024x1024 image
void write_tga(char* fname, unsigned char *image) {
  int fd = open(fname, O_CREAT | O_RDWR, 0644);
  write(fd, tga, sizeof(tga));
  printf("Created file %s: Writing pixel size %d bytes\n", fname, WIDTH * HEIGHT);
  write(fd, image, WIDTH * HEIGHT);
  close(fd);
}

// create a random filter
void random_filter() {
  filter.sum = 0;
  for (int i = 0; i < KSIZE; i++) {
    filter.values[i] = rand() % 10;
    filter.sum += filter.values[i];
  }
}

void gaussian_3x3() {
  int values[] = { 1, 2, 1,
                   2, 4, 2,
                   1, 2, 1 };

  memcpy(filter.values, values, KSIZE * sizeof(int));

  filter.sum = 16;
}

void gaussian_7x7() {
  int values[] = { 0,  0,  1,   2,  1,  0, 0,
                   0,  3, 13,  22, 13,  3, 0,
                   1, 13, 59,  97, 59, 13, 1,
                   2, 22, 97, 159, 97, 22, 2,
                   1, 13, 59,  97, 59, 13, 1,
                   0,  3, 13,  22, 13,  3, 0,
                   0,  0,  1,   2,  1,  0, 0 };

  memcpy(filter.values, values, KSIZE * sizeof(int));

  filter.sum = 0;
  for (int i = 0; i < KSIZE; i++)
    filter.sum += filter.values[i];
}

// create a Gaussian filter
void gaussian_filter() {
  if (KSIZE == 9) gaussian_3x3();
  else if (KSIZE == 49) gaussian_7x7();
}

// create a horizonal filter
void horizontal_filter() {
  for (int i = 0; i < KSIZE; i++) {
    int row = i / KLEN;
    if (3 * row < KLEN)
      filter.values[i] = -1;
    else if (KLEN < 2 * row)
      filter.values[i] = 1;
    else
      filter.values[i] = 0;
  }

  filter.sum = 0;
}

// create a vertical filter
void vertical_filter() {
  for (int i = 0; i < KSIZE; i++) {
    int col = i % KLEN;
    if (3 * col < KLEN)
      filter.values[i] = -1;
    else if (KLEN < 2 * col)
      filter.values[i] = 1;
    else
      filter.values[i] = 0;
  }

  filter.sum = 0;
}

void interchange() {
  unsigned char* aux = pixels;
  pixels = target;
  target = aux;
}



// compute the target pixel at (x,y)
void compute_target_pixel(int x, int y) {
  int i, j, sum = 0;
  int delta = (KLEN - 1) / 2;
  for (i = -delta; i <= delta; ++i)
    for (j = -delta; j <= delta; ++j)
      if (0 <= x + i && x + i < WIDTH && 0 <= y + j && y + j < HEIGHT)
        sum += filter.values[(i + delta) * KLEN + (j + delta)] * pixels[(x + i) * HEIGHT + (y + j)];

  if(filter.sum > 0) target[x * HEIGHT + y] = sum / filter.sum;
  else target[x * HEIGHT + y] = sum;
}

void convolve() {
  int x, y;
  for (x = 0; x < WIDTH; ++x)
    for (y = 0; y < HEIGHT; ++y)
      compute_target_pixel(x, y);
}

void multi_convolve_1(int iter) {
  printf("\n\n1.- Single Thread Convolution\n\n");
  for(int i = 0; i < iter; i++){ 
    printf("Iteration nº%d.\n",i+1);
    convolve(); // Realizamos tantas convoluciones como iteraciones se nos pide.
    interchange(); // Intercambiamos el output de una convulución por el input de la siguiente.
  }
}

void* convolveThread(void* param){
  int* args = (int*)param; // Lo casteamos a un array de ints.
  int id = (int)args[0]; // El id del thread creado
  int n_threads = (int)args[1]; // Número de threads creados
  int rows_thread =  HEIGHT/n_threads; // Número de filas que tiene que hacer un mismo thread
  int start = id*rows_thread; // Fila de inicio
  int end = start+rows_thread; // ültima fila, no incluida
 
  //printf("Start: %d, end: %d\n",start,end);
  //printf("Convolving...\n");
  for (int x = 0; x < WIDTH; ++x){
    for (int y = start; y < end; ++y){
      compute_target_pixel(x, y);
    }
  }
  free(param);
}

void convolve_threaded(int n_threads){
  pthread_t* tids = malloc(sizeof(pthread_t)*n_threads); // Guarda dinámicamente los thread ids
  for(int i = 0; i < n_threads; i++){ // Crea el número deseado de threads
    //printf("Creating threads...\n");
    int* param = malloc(sizeof(int)*2); // Almacena dinámicamente los parámetros de entrada
    param[0] = i; // Este será el identificador del  thread para indicar que filas debe calcular
    param[1] = n_threads; 
    pthread_create(&tids[i],NULL,convolveThread,param); // Se crea el thread y se pasa a la función convolve
  }
  for(int i = 0; i < n_threads; i++){
   pthread_join(tids[i], NULL);
  }
  free(tids);
}

void multi_convolve_2(int iter, int n_threads) {
  printf("\n\n2.- Multi Thread convolution (%d threads)\n\n",n_threads);
  for(int i = 0; i < iter; i++){ 
    printf("Iteration nº%d.\n",i+1);
    convolve_threaded(n_threads); // Realizamos tantas convoluciones como iteraciones se nos pide.
    interchange(); // Intercambiamos el output de una convulución por el input de la siguiente.
  }
}


void* multiConvolveThread(void* param){
  int* args = (int*)param; // Lo casteamos a un array de ints.
  int id = (int)args[0]; // El id del thread creado
  int n_threads = (int)args[1]; // Número de threads creados
  int iter = (int)args[2]; // Número de convoluciones
  int rows_thread =  HEIGHT/n_threads; // Número de filas que tiene que hacer un mismo thread
  int start = id*rows_thread; // Fila de inicio
  int end = start+rows_thread; // ültima fila, no incluida

  for (int i = 0; i < iter; ++i){ // Número de convoluciones que se realizan
    b_wait_all(&barrier_image); // Todos los threads esperan al que está cambiando la imagen para no sobreescribirse
    all_ended = false; // Se restablece el booleano como false
    //b_reset(&barrier_image,n_threads); // Debería restablecer la barrera, pero no funciona
    for (int x = 0; x < WIDTH; ++x){ // Convolución...
      for (int y = start; y < end; ++y){
        compute_target_pixel(x, y);
      }
    }
    //printf("\nThread id: %d, iteration %d.\n",id,i);
    b_wait_all(&barrier_conv); // Espera a que todos los threads hayan completado la convolución para cambier la imagen
    if(all_ended){ // Este booleano se establece como true tras completar b_wait_all
      all_ended = false; // Se restablece como false
      //printf("Thread %d changing images.\n",id);
      interchange(); // Intercabia las imagenes
      
    }
    //b_reset(&barrier_conv,n_threads);  // Debería restablecer la barrera, pero no funciona
  }
  free(param);
}

void multi_convolve_3(int iter, int n_threads) {
  printf("\n\n3.- Multi Thread convolution synchronized (%d threads)\n\n",n_threads);
  pthread_t* tids = malloc(sizeof(pthread_t)*n_threads); // Guarda dinámicamente los thread ids
  b_create(&barrier_conv, n_threads); // Crea dos barreras una para la convolución y otra para la imagen
  b_create(&barrier_image, n_threads);
  for(int i = 0; i < n_threads; i++){ // Crea el número deseado de threads
    int* param = malloc(sizeof(int)*3); // Almacena dinámicamente los parámetros de entrada
    param[0] = i; // Este será el identificador del  thread para indicar que filas debe calcular
    param[1] = n_threads; 
    param[2] = iter;
    pthread_create(&tids[i],NULL,multiConvolveThread,param); // Se crea el thread y se pasa a la función convolve
  }
  for(int i = 0; i < n_threads; i++){
   pthread_join(tids[i], NULL);
  }
  free(tids);
}



void load_image() {
  read_tga("test.tga");  // read the image
  memcpy(target, pixels, WIDTH*HEIGHT);
}

int main(void) {
  // Allocate images
  pixels = malloc(WIDTH * HEIGHT);
  target = malloc(WIDTH * HEIGHT);

  load_image();

  // create a filter
  gaussian_filter();
  //vertical_filter();
  //horizontal_filter();

  load_image();
  
  // Params
  int iter = 20;
  int n_threads = 32;
  
  //Single Thread
  startTimer(0);
  multi_convolve_1(iter);
  long t1 = endTimer(0);
  write_tga("output1.tga", target);

  //Multi Thread
  load_image();
  startTimer(1);
  multi_convolve_2(iter,n_threads);
  long t2 = endTimer(1);
  write_tga("output2.tga", target);

  //Multi Thread Synchronized
  load_image();
  startTimer(2);
  multi_convolve_3(iter,n_threads);
  long t3 = endTimer(2);
  write_tga("output3.tga", target);

  //Benchmark
  printf("\nTime single-thread convolution: %ldms\n",t1);
  printf("Time %d-threads convolution: %ldms\n",n_threads,t2);
  printf("Time %d-threads convolution synchronized: %ldms\n",n_threads,t3);
  
  // write the convolved image

  free(pixels);
  free(target);
  return 0;
}
 
