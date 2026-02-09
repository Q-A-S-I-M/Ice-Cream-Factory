#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/mman.h>
#include<string.h>
#include<fcntl.h>
#include<semaphore.h>
#include<pthread.h>

#define SHM_NAME "/icecream_shm"
#define BUFFER_SIZE 10

typedef enum {
    VANILLA, CHOCOLATE, STRAWBERRY, MANGO,
    COOKIES_AND_CREAM, BUTTERSCOTCH, PISTACHIO, CARAMEL
} Flavor;

typedef struct {
    int orderid;
    char custname[50];
    int quantity;
    Flavor flavor;
    int isMixed;
    int isFrozen;
    int isPackaged;
    pid_t customer_pid;
} Order;

typedef struct{
  Order iceCreamOrders[10];
  char sem_name_full[50]; 
  char sem_name_empty[50]; 
  pthread_mutex_t order_mutex;
  int count;
  int order_in;
  int order_out;
}SharedMemory;

int main(){
   int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    ftruncate(shm_fd, sizeof(SharedMemory));
    SharedMemory* shm_ptr = mmap(0, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    shm_ptr->order_in=0;
    shm_ptr->order_out=0;
    shm_ptr->count=0;
    
    pthread_mutex_init(&shm_ptr->order_mutex, NULL);
    
    const char* sem_name_in = "/order_semaphore_in"; 
    const char* sem_name_out = "/order_semaphore_out";  

    // Create named semaphore with initial value 0
    sem_t* sem1 = sem_open(sem_name_in, O_CREAT | O_EXCL, 0666, 0);
    sem_t* sem2 = sem_open(sem_name_out, O_CREAT | O_EXCL, 0666, BUFFER_SIZE);

    if (sem1 == SEM_FAILED) {
        perror("sem_open_full failed");
        exit(EXIT_FAILURE);
    }
    
    if (sem2 == SEM_FAILED) {
        perror("sem_open_empty failed");
        exit(EXIT_FAILURE);
    }
    
    strncpy(shm_ptr->sem_name_full, sem_name_in, sizeof(shm_ptr->sem_name_full));
    shm_ptr->sem_name_full[sizeof(shm_ptr->sem_name_full) - 1] = '\0';

    strncpy(shm_ptr->sem_name_empty, sem_name_out, sizeof(shm_ptr->sem_name_empty));
    shm_ptr->sem_name_empty[sizeof(shm_ptr->sem_name_empty) - 1] = '\0';
   
    printf("Semaphores created and initialized.\n");
}
