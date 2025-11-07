#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 100
#define TRANSACTIONS_PER_THREAD 10

int balance = 1000;
// TODO: Add a mutex here

void* deposit(void* arg) {
    for (int i = 0; i < TRANSACTIONS_PER_THREAD; i++) {
        // TODO: Add locking here
        int temp = balance;
        usleep(1); // Simulate some processing
        balance = temp + 10;
        // TODO: Add unlocking here
    }
    return NULL;
}

void* withdraw(void* arg) {
    for (int i = 0; i < TRANSACTIONS_PER_THREAD; i++) {
        // TODO: Add locking here
        int temp = balance;
        usleep(1); // Simulate some processing
        balance = temp - 10;
        // TODO: Add unlocking here
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS * 2];
    
    printf("Starting balance: %d\n", balance);
    
    // TODO: Initialize mutex
    
    // Create deposit threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, deposit, NULL);
    }
    
    // Create withdraw threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[NUM_THREADS + i], NULL, withdraw, NULL);
    }
    
    // Wait for all threads
    for (int i = 0; i < NUM_THREADS * 2; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Final balance: %d\n", balance);
    
    if (balance == 1000) {
        printf("Success! Balance is correct.\n");
    } else {
        printf("Bug! Expected 1000 but got %d\n", balance);
        printf("Hint: You need to protect the balance variable with a mutex!\n");
    }
    
    // TODO: Destroy mutex
    
    return 0;
}
