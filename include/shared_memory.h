#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <pthread.h>
#include <semaphore.h>
#include "cashier.h" //do max cashiers

typedef struct {
    int should_exit;  // Flaga zakończenia
    sem_t semaphore;  // Semafor dla synchronizacji
    int queue_ids[MAX_CASHIERS]; // Tablica kolejki komunikatów
    int number_customer; //obecna ilsoc klientow w sklepie
} SharedMemory;

SharedMemory* init_shared_memory();
void cleanup_shared_memory(SharedMemory* shared_mem);
void set_should_exit(SharedMemory* shared_mem, int value);
int get_should_exit(SharedMemory* shared_mem);
void set_queue_id(SharedMemory* shared_mem, int cashier_id, int queue_id);
int get_queue_id(SharedMemory* shared_mem, int cashier_id) ;
void increment_number_customer(SharedMemory* shared_mem);
void decrement_number_customer(SharedMemory* shared_mem);



#endif  // SHARED_MEMORY_H
