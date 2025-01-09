// shared_memory.c
#include <sys/shm.h>
#include <sys/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "shared_memory.h"  // Załączanie nagłówka, który zawiera SharedMemory

#define SHM_KEY 12345  // Klucz do pamięci dzielonej

SharedMemory* init_shared_memory() {
    int shm_id = shmget(SHM_KEY, sizeof(SharedMemory), 0666 | IPC_CREAT);
    if (shm_id == -1) {
        perror("Błąd tworzenia pamięci dzielonej");
        exit(1);
    }

    SharedMemory* shared_mem = (SharedMemory*)shmat(shm_id, NULL, 0);
    if (shared_mem == (void*)-1) {
        perror("Błąd przypisania pamięci dzielonej");
        exit(1);
    }

    shared_mem->should_exit = 0;
    shared_mem->number_customer = 0;
    if (sem_init(&shared_mem->semaphore, 1, 1) != 0) {  // Semafor współdzielony między procesami
        perror("Błąd inicjalizacji semafora");
        exit(1);
    }

    // Inicjalizacja tablicy queue_ids
    for (int i = 0; i < MAX_CASHIERS; i++) {
        shared_mem->queue_ids[i] = -1;  // Wartość -1 oznacza brak kolejki
    }

    return shared_mem;
}


void cleanup_shared_memory(SharedMemory* shared_mem) {
    if (shmdt(shared_mem) == -1) {
        perror("Błąd odłączania pamięci dzielonej");
    }

    int shm_id = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shm_id != -1) {
        if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
            perror("Błąd usuwania pamięci dzielonej");
        } else {
            printf("Pamięć dzielona usunięta\n");
        }
    }

    sem_destroy(&shared_mem->semaphore);  // Zniszczenie semafora
}


// Funkcja do ustawienia flagi zakończenia w pamięci dzielonej
void set_should_exit(SharedMemory* shared_mem, int value) {
    sem_wait(&shared_mem->semaphore);
    shared_mem->should_exit = value;
    sem_post(&shared_mem->semaphore);
}

// Funkcja do odczytu flagi zakończenia
int get_should_exit(SharedMemory* shared_mem) {
    sem_wait(&shared_mem->semaphore);
    int value = shared_mem->should_exit;
    sem_post(&shared_mem->semaphore);
    return value;
}


void set_queue_id(SharedMemory* shared_mem, int cashier_id, int queue_id) {
    sem_wait(&shared_mem->semaphore);
    shared_mem->queue_ids[cashier_id] = queue_id;
    sem_post(&shared_mem->semaphore);
}

int get_queue_id(SharedMemory* shared_mem, int cashier_id) {
    sem_wait(&shared_mem->semaphore);
    int queue_id = shared_mem->queue_ids[cashier_id];
    sem_post(&shared_mem->semaphore);
    return queue_id;
}


void increment_number_customer(SharedMemory* shared_mem) {
    sem_wait(&shared_mem->semaphore);  // Czekamy na dostęp do sekcji krytycznej
    shared_mem->number_customer++;     // Zwiększamy liczbę klientów
    sem_post(&shared_mem->semaphore);  // Zwalniamy sekcję krytyczną
}


void decrement_number_customer(SharedMemory* shared_mem) {
    sem_wait(&shared_mem->semaphore);  // Czekamy na dostęp do sekcji krytycznej
    shared_mem->number_customer--;     // Zmniejszamy liczbę klientów
    sem_post(&shared_mem->semaphore);  // Zwalniamy sekcję krytyczną
}
