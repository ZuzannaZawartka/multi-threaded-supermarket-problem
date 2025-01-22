#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <pthread.h>
#include <semaphore.h>
#include "main.h"

typedef struct {
    int queue_ids[MAX_CASHIERS]; // Tablica kolejki komunikatów
    int active_cashiers;  //ilosc aktywnych kasjerow z zakresu ktorego moga wybierac nowi klienci
    int customer_count;
} SharedMemory;

//zarządzanie pamięcią dzielona
SharedMemory* init_shared_memory();
SharedMemory* get_shared_memory() ;
void cleanup_shared_memory(SharedMemory* shared_mem);

//funkcje działające na tablicach kolejek komunikatów z shared memory
void set_queue_id(SharedMemory* shared_mem, int cashier_id, int queue_id);
int get_queue_id(SharedMemory* shared_mem, int cashier_id);

//zarządzanie semaforem do shared memory
void cleanup_semaphore();
sem_t* get_semaphore();

//zarządzanie zmienną active_cashier która określa zakres dostępnych kasjerów
int get_active_cashiers(SharedMemory* shared_mem);
void decrement_active_cashiers(SharedMemory* shared_mem);
void increment_active_cashiers(SharedMemory* shared_mem);


void decrement_customer_count(SharedMemory* shared_mem) ;
void increment_customer_count(SharedMemory* shared_mem);
int get_customer_count(SharedMemory* shared_mem);
#endif  
