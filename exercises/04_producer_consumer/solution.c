#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 5
#define NUM_ITEMS 20
#define NUM_PRODUCERS 2
#define NUM_CONSUMERS 2

typedef struct {
    int buffer[BUFFER_SIZE];
    int count;
    int in;
    int out;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} bounded_buffer_t;

bounded_buffer_t bb;
int produced_count = 0;
int consumed_count = 0;

void buffer_init(bounded_buffer_t* bb) {
    bb->count = 0;
    bb->in = 0;
    bb->out = 0;
    pthread_mutex_init(&bb->mutex, NULL);
    pthread_cond_init(&bb->not_full, NULL);
    pthread_cond_init(&bb->not_empty, NULL);
}

void* producer(void* arg) {
    long id = (long)arg;
    
    while (1) {
        pthread_mutex_lock(&bb.mutex);
        
        if (produced_count >= NUM_ITEMS) {
            pthread_mutex_unlock(&bb.mutex);
            break;
        }
        
        // Wait while buffer is full (MUST use while for spurious wakeups)
        while (bb.count >= BUFFER_SIZE) {
            pthread_cond_wait(&bb.not_full, &bb.mutex);
        }
        
        // Produce item
        int item = ++produced_count;
        bb.buffer[bb.in] = item;
        bb.in = (bb.in + 1) % BUFFER_SIZE;
        bb.count++;
        
        printf("Producer %ld produced: %d\n", id, item);
        
        // Signal consumers
        pthread_cond_signal(&bb.not_empty);
        
        pthread_mutex_unlock(&bb.mutex);
        usleep(10000);
    }
    
    // Wake up any waiting consumers
    pthread_cond_broadcast(&bb.not_empty);
    return NULL;
}

void* consumer(void* arg) {
    long id = (long)arg;
    
    while (1) {
        pthread_mutex_lock(&bb.mutex);
        
        // Wait while buffer is empty (MUST use while for spurious wakeups)
        while (bb.count == 0 && consumed_count < NUM_ITEMS) {
            pthread_cond_wait(&bb.not_empty, &bb.mutex);
        }
        
        if (consumed_count >= NUM_ITEMS) {
            pthread_mutex_unlock(&bb.mutex);
            break;
        }
        
        // Consume item
        int item = bb.buffer[bb.out];
        bb.out = (bb.out + 1) % BUFFER_SIZE;
        bb.count--;
        consumed_count++;
        
        printf("Consumer %ld consumed: %d\n", id, item);
        
        // Signal producers
        pthread_cond_signal(&bb.not_full);
        
        pthread_mutex_unlock(&bb.mutex);
        usleep(15000);
    }
    
    return NULL;
}

int main() {
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    
    buffer_init(&bb);
    
    for (long i = 0; i < NUM_PRODUCERS; i++) {
        pthread_create(&producers[i], NULL, producer, (void*)i);
    }
    
    for (long i = 0; i < NUM_CONSUMERS; i++) {
        pthread_create(&consumers[i], NULL, consumer, (void*)i);
    }
    
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }
    
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
    }
    
    if (consumed_count == NUM_ITEMS) {
        printf("\nAll items produced and consumed successfully!\n");
    } else {
        printf("\nError: Expected %d items but consumed %d\n", NUM_ITEMS, consumed_count);
    }
    
    pthread_mutex_destroy(&bb.mutex);
    pthread_cond_destroy(&bb.not_full);
    pthread_cond_destroy(&bb.not_empty);
    
    return 0;
}
