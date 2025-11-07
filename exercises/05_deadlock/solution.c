#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct {
    int id;
    int balance;
    pthread_mutex_t mutex;
} account_t;

account_t accounts[2];
int transfer_count = 0;
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

void account_init(account_t* acc, int id, int balance) {
    acc->id = id;
    acc->balance = balance;
    pthread_mutex_init(&acc->mutex, NULL);
}

// Deadlock-free version using lock ordering
void transfer_safe(account_t* from, account_t* to, int amount) {
    // Always lock in consistent order (lower ID first)
    account_t* first = (from->id < to->id) ? from : to;
    account_t* second = (from->id < to->id) ? to : from;
    
    pthread_mutex_lock(&first->mutex);
    pthread_mutex_lock(&second->mutex);
    
    from->balance -= amount;
    to->balance += amount;
    
    printf("Transferring $%d from account %d to account %d\n", 
           amount, from->id, to->id);
    
    pthread_mutex_unlock(&second->mutex);
    pthread_mutex_unlock(&first->mutex);
}

void* worker(void* arg) {
    long id = (long)arg;
    
    for (int i = 0; i < 5; i++) {
        int amount = 10 + (rand() % 40);
        
        if (id == 0) {
            transfer_safe(&accounts[0], &accounts[1], amount);
        } else {
            transfer_safe(&accounts[1], &accounts[0], amount);
        }
        
        pthread_mutex_lock(&count_mutex);
        transfer_count++;
        pthread_mutex_unlock(&count_mutex);
        
        usleep(50000);
    }
    
    return NULL;
}

int main() {
    pthread_t threads[2];
    
    account_init(&accounts[0], 0, 1000);
    account_init(&accounts[1], 1, 1000);
    
    printf("Starting balances: Account 0: $%d, Account 1: $%d\n\n", 
           accounts[0].balance, accounts[1].balance);
    
    for (long i = 0; i < 2; i++) {
        pthread_create(&threads[i], NULL, worker, (void*)i);
    }
    
    for (int i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("\nAll transfers completed successfully!\n");
    printf("Final balances: Account 0: $%d, Account 1: $%d\n", 
           accounts[0].balance, accounts[1].balance);
    
    pthread_mutex_destroy(&accounts[0].mutex);
    pthread_mutex_destroy(&accounts[1].mutex);
    
    return 0;
}
