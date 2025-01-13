#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <pthread.h>
#include <semaphore.h>
#include "cashier.h" //do max cashiers

#define MAX_CASHIERS 10

typedef struct {
    int queue_ids[MAX_CASHIERS]; // Tablica kolejki komunikatów
} SharedMemory;

SharedMemory* init_shared_memory();
void cleanup_shared_memory(SharedMemory* shared_mem);
void set_queue_id(SharedMemory* shared_mem, int cashier_id, int queue_id);
int get_queue_id(SharedMemory* shared_mem, int cashier_id);
SharedMemory* get_shared_memory() ;
void cleanup_semaphore();
sem_t* get_semaphore();

#endif  
