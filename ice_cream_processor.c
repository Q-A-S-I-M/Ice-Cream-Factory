#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <errno.h>
#include<signal.h>

#define SHM_NAME "/icecream_shm"
#define BUFFER_SIZE 10
#define MACHINE_BUFFER_SIZE 5
#define THREADS_PER_MACHINE 3

typedef enum {
    VANILLA, CHOCOLATE, STRAWBERRY, MANGO,
    COOKIES_AND_CREAM, BUTTERSCOTCH, PISTACHIO, CARAMEL
} Flavor;

const char* flavor_names[] = {
    "Vanilla", "Chocolate", "Strawberry", "Mango",
    "Cookies and Cream", "Butterscotch", "Pistachio", "Caramel"
};

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

typedef struct {
    Order iceCreamOrders[BUFFER_SIZE];
    char sem_name_full[50];
    char sem_name_empty[50];
    pthread_mutex_t order_mutex;
    int count;
    int order_in;
    int order_out;
} SharedMemory;

typedef struct {
    Order buffer[MACHINE_BUFFER_SIZE];
    int in, out;
    sem_t full, empty;
    pthread_mutex_t mutex;
} MachineBuffer;

SharedMemory* shm_ptr;
MachineBuffer machine1, machine2, machine3;
sem_t *sem_full, *sem_empty;

void init_machine_buffer(MachineBuffer* m) {
    m->in = m->out = 0;
    sem_init(&m->full, 0, 0);
    sem_init(&m->empty, 0, MACHINE_BUFFER_SIZE);
    pthread_mutex_init(&m->mutex, NULL);
}

void* producer_thread(void* arg) {
    while (1) {
        sem_wait(sem_full);

        pthread_mutex_lock(&shm_ptr->order_mutex);
        Order order = shm_ptr->iceCreamOrders[shm_ptr->order_out];
        shm_ptr->order_out = (shm_ptr->order_out + 1) % BUFFER_SIZE;
        pthread_mutex_unlock(&shm_ptr->order_mutex);

        sem_post(sem_empty);

        sem_wait(&machine1.empty);
        pthread_mutex_lock(&machine1.mutex);
        machine1.buffer[machine1.in] = order;
        machine1.in = (machine1.in + 1) % MACHINE_BUFFER_SIZE;
        printf("[Producer] Order #%d placed into Mixing Machine.\n", order.orderid);
        pthread_mutex_unlock(&machine1.mutex);
        sem_post(&machine1.full);

        sleep(1);
    }
    return NULL;
}

typedef struct {
    MachineBuffer* in_buf;
    MachineBuffer* out_buf;
    const char* step;
} MachineArgs;

void* machine_worker(void* arg) {
    MachineArgs* args = (MachineArgs*)arg;
    while (1) {
        sem_wait(&args->in_buf->full);
        pthread_mutex_lock(&args->in_buf->mutex);
        Order order = args->in_buf->buffer[args->in_buf->out];
        args->in_buf->out = (args->in_buf->out + 1) % MACHINE_BUFFER_SIZE;
        pthread_mutex_unlock(&args->in_buf->mutex);
        sem_post(&args->in_buf->empty);

        if (strcmp(args->step, "Mixing") == 0) {
            printf("\n🌀[Mixing] Mixing flavor '%s' for Order #%d...\n",
                   flavor_names[order.flavor], order.orderid);
            sleep(2);
            order.isMixed = 1;
        } else if (strcmp(args->step, "Freezing") == 0) {
            printf("\n❄️[Freezing] Freezing Order #%d...\n", order.orderid);
            sleep(2);
            order.isFrozen = 1;
        } else if (strcmp(args->step, "Packaging") == 0) {
            printf("\n📦[Packaging] Packaging Order #%d...\n", order.orderid);
            sleep(2);
            order.isPackaged = 1;
        }

        if (args->out_buf) {
            sem_wait(&args->out_buf->empty);
            pthread_mutex_lock(&args->out_buf->mutex);
            args->out_buf->buffer[args->out_buf->in] = order;
            args->out_buf->in = (args->out_buf->in + 1) % MACHINE_BUFFER_SIZE;
            pthread_mutex_unlock(&args->out_buf->mutex);
            sem_post(&args->out_buf->full);
            printf("✅ [%s → Next] Order #%d moved to next stage.\n", args->step, order.orderid);
        } else {
            printf("🎉 [DONE] Order #%d has been packaged and is ready!\n", order.orderid);
            kill(order.customer_pid, SIGUSR1);
        }
    }
    return NULL;
}

int main() {
    printf("=============================================\n");
    printf("   🍦 Ice Cream Factory Order Processing\n");
    printf("=============================================\n\n");
    
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    shm_ptr = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    sem_full = sem_open(shm_ptr->sem_name_full, 0);
    sem_empty = sem_open(shm_ptr->sem_name_empty, 0);
    if (sem_full == SEM_FAILED || sem_empty == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    init_machine_buffer(&machine1);
    init_machine_buffer(&machine2);
    init_machine_buffer(&machine3);

    pthread_t producer;
    pthread_create(&producer, NULL, producer_thread, NULL);

    MachineArgs args1 = { &machine1, &machine2, "Mixing" };
    MachineArgs args2 = { &machine2, &machine3, "Freezing" };
    MachineArgs args3 = { &machine3, NULL,      "Packaging" };

    pthread_t mix_threads[THREADS_PER_MACHINE];
    pthread_t freeze_threads[THREADS_PER_MACHINE];
    pthread_t pack_threads[THREADS_PER_MACHINE];

    for (int i = 0; i < THREADS_PER_MACHINE; ++i) {
        pthread_create(&mix_threads[i], NULL, machine_worker, &args1);
        pthread_create(&freeze_threads[i], NULL, machine_worker, &args2);
        pthread_create(&pack_threads[i], NULL, machine_worker, &args3);
    }

    pthread_join(producer, NULL);
    return 0;
}

