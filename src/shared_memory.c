#include <sys/shm.h>
#include <sys/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>  // Załączamy semafor

#include "shared_memory.h"

#define SHM_KEY 12345  // Klucz do pamięci dzielonej

// Inicjalizowanie pamięci dzielonej z semaforem POSIX
SharedMemory* init_shared_memory(sem_t* shared_mem_semaphore) {
    // Tworzenie pamięci dzielonej
    int shm_id = shmget(SHM_KEY, sizeof(SharedMemory), 0666 | IPC_CREAT);
    if (shm_id == -1) {
        perror("Błąd tworzenia pamięci dzielonej");
        exit(1);
    }

    // Przyłączenie do pamięci dzielonej
    SharedMemory* shared_mem = (SharedMemory*)shmat(shm_id, NULL, 0);
    if (shared_mem == (void*)-1) {
        perror("Błąd przypisania pamięci dzielonej");
        exit(1);
    }

    // Inicjalizacja zmiennych w pamięci dzielonej
    shared_mem->should_exit = 0;
    shared_mem->number_customer = 0;

    // Przypisanie semafora, który będzie używany przez wszystkie wątki i procesy
    shared_mem->semaphore = shared_mem_semaphore;

    // Inicjalizacja tablicy queue_ids
    for (int i = 0; i < MAX_CASHIERS; i++) {
        shared_mem->queue_ids[i] = -1;  // Wartość -1 oznacza brak kolejki
    }

    return shared_mem;
}

// Czyszczenie pamięci dzielonej
void cleanup_shared_memory(SharedMemory* shared_mem) {
    if (shmdt(shared_mem) == -1) {
        perror("Błąd odłączania pamięci dzielonej");
    }

    // Usunięcie pamięci dzielonej
    int shm_id = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shm_id != -1) {
        if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
            perror("Błąd usuwania pamięci dzielonej");
        } else {
            printf("Pamięć dzielona usunięta\n");
        }
    }
}

// Ustawienie flagi "should_exit" z synchronizacją semaforem
void set_should_exit(SharedMemory* shared_mem, int value) {
    sem_wait(shared_mem->semaphore);  
    shared_mem->should_exit = value;
    sem_post(shared_mem->semaphore);  
}


int get_should_exit(SharedMemory* shared_mem) {
    sem_wait(shared_mem->semaphore);  
    int value = shared_mem->should_exit;
    sem_post(shared_mem->semaphore);  
    return value;
}

// Ustawienie identyfikatora kolejki kasjera z synchronizacją semaforem
void set_queue_id(SharedMemory* shared_mem, int cashier_id, int queue_id) {
    sem_wait(shared_mem->semaphore);  
    shared_mem->queue_ids[cashier_id] = queue_id;
    sem_post(shared_mem->semaphore);  
}

// Pobranie identyfikatora kolejki kasjera z synchronizacją semaforem
int get_queue_id(SharedMemory* shared_mem, int cashier_id) {
    sem_wait(shared_mem->semaphore);  
    int queue_id = shared_mem->queue_ids[cashier_id];
    sem_post(shared_mem->semaphore);  
    return queue_id;
}

// Inkrementacja liczby klientów w sklepie z synchronizacją semaforem
void increment_number_customer(SharedMemory* shared_mem) {
    sem_wait(shared_mem->semaphore);  
    shared_mem->number_customer++;     // Zwiększamy liczbę klientów
    sem_post(shared_mem->semaphore);  
}

// Dekrementacja liczby klientów w sklepie z synchronizacją semaforem
void decrement_number_customer(SharedMemory* shared_mem) {
    sem_wait(shared_mem->semaphore);  
    shared_mem->number_customer--;     // Zmniejszamy liczbę klientów
    sem_post(shared_mem->semaphore);  
}

// Pobranie liczby aktualnych klientów z synchronizacją semaforem
int get_number_customer(SharedMemory* shared_mem) {
    sem_wait(shared_mem->semaphore);  
    int current_customers = shared_mem->number_customer;  // Zapisujemy aktualną liczbę klientów
    sem_post(shared_mem->semaphore);  
    return current_customers;  
}


void set_active_cashiers(SharedMemory* shared_mem, int value) {
    sem_wait(shared_mem->semaphore);  
    shared_mem->active_cashiers = value; 
    sem_post(shared_mem->semaphore);  
}

int get_active_cashiers(SharedMemory* shared_mem) {
    sem_wait(shared_mem->semaphore); 
    int active_cashiers = shared_mem->active_cashiers; 
    sem_post(shared_mem->semaphore);  
    return active_cashiers;
}