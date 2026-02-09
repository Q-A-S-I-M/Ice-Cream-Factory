#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/mman.h>
#include<string.h>
#include<fcntl.h>
#include<semaphore.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

#define SHM_NAME "/icecream_shm"
#define BUFFER_SIZE 10

typedef enum {
    VANILLA,
    CHOCOLATE,
    STRAWBERRY,
    MANGO,
    COOKIES_AND_CREAM,
    BUTTERSCOTCH,
    PISTACHIO,
    CARAMEL
} Flavor;

typedef struct{
  int orderid;
  char custname[50];
  int quantity;
  Flavor flavor;
  int isMixed;
  int isFrozen;
  int isPackaged;
  pid_t customer_pid;
}Order;

typedef struct{
  Order iceCreamOrders[BUFFER_SIZE];
  char sem_name_full[50]; 
  char sem_name_empty[50]; 
  pthread_mutex_t order_mutex;
  int count;
  int order_in;
  int order_out;
}SharedMemory;

const char* getFlavorName(Flavor flavor) {
    switch (flavor) {
        case VANILLA: return "Vanilla";
        case CHOCOLATE: return "Chocolate";
        case STRAWBERRY: return "Strawberry";
        case MANGO: return "Mango";
        case COOKIES_AND_CREAM: return "Cookies and Cream";
        case BUTTERSCOTCH: return "Butterscotch";
        case PISTACHIO: return "Pistachio";
        case CARAMEL: return "Caramel";
        default: return "Unknown";
    }
}

void handle_sigusr1(int sig) {
    printf("[PID %d] Your order has been completed! Enjoy your ice cream.\n", getpid());
}

int main(){
   signal(SIGUSR1, handle_sigusr1);
   int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    SharedMemory* shm_ptr = mmap(0, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    sem_t* sem_full = sem_open(shm_ptr->sem_name_full, 0);  // 0 = open existing only
    sem_t* sem_empty = sem_open(shm_ptr->sem_name_empty, 0);
    
    if (sem_full == SEM_FAILED || sem_empty == SEM_FAILED) {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }
    
    Order order;
    
        printf("Enter customer name: ");
        fgets(order.custname, sizeof(order.custname), stdin);
        order.custname[strcspn(order.custname, "\n")] = '\0';

        printf("Select a flavor:\n");
        printf("0: Vanilla\n1: Chocolate\n2: Strawberry\n3: Mango\n4: Cookies and Cream\n5: Butterscotch\n6: Pistachio\n7: Caramel\n");
        int flavor_choice;
        scanf("%d", &flavor_choice);
        order.flavor = (Flavor)flavor_choice;

        printf("Enter quantity: ");
        scanf("%d", &order.quantity);
        
        order.customer_pid = getpid(); 
        
    if (sem_trywait(sem_empty) == -1) {
         if (errno == EAGAIN) {
            printf("[PID %d] Sorry! you will have to wait for a while as the order limit is reached...\n", getpid());
            sem_wait(sem_empty);
         } else {
            perror("sem_trywait error");
            exit(EXIT_FAILURE);
         }
    } else {
        printf("[PID %d] Found space — placing order.\n", getpid());
    }

    printf("[PID %d] Placing ice cream order in shared memory...\n", getpid());

    pthread_mutex_lock(&shm_ptr->order_mutex);
    shm_ptr->count++;
    order.orderid = shm_ptr->count;
    shm_ptr->iceCreamOrders[shm_ptr->order_in] = order;

    printf("Order placed at index %d for %s (%s)\n", shm_ptr->order_in, order.custname, getFlavorName(order.flavor));

    shm_ptr->order_in = (shm_ptr->order_in + 1) % BUFFER_SIZE;
    pthread_mutex_unlock(&shm_ptr->order_mutex);

    sleep(1);
    sem_post(sem_full);

    printf("[PID %d] Waiting for order to be completed...\n", getpid());
    pause();  // wait for SIGUSR1

    return 0;
             
}
