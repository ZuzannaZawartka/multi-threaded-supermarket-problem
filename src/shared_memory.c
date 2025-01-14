#include <sys/shm.h>
#include <sys/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>  // Załączamy semafor
#include <sys/msg.h>  // Nagłówek do obsługi kolejek komunikatów
#include "shared_memory.h"
#include <fcntl.h>

#define SHM_KEY 12345  // Klucz do pamięci dzielonej
#define SEM_NAME "/shared_mem_semaphore" 
// Inicjalizowanie pamięci dzielonej z semaforem POSIX
SharedMemory* init_shared_memory() {
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

    // Tworzymy semafor do shared memory, jeśli jeszcze nie istnieje
    sem_t* shared_mem_semaphore = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (shared_mem_semaphore == SEM_FAILED) {
        perror("Błąd tworzenia semafora");
        exit(1);
    }

   // Zwolnienie semafora
    if (sem_post(shared_mem_semaphore) == -1) {
        perror("Błąd zwolnienia semafora");
        exit(1);
    }

    // Inicjalizacja tablicy queue_ids
    for (int i = 0; i < MAX_CASHIERS; i++) {
        shared_mem->queue_ids[i] = -1;  // Wartość -1 oznacza brak kolejki
    }

    return shared_mem;
}

void set_queue_id(SharedMemory* shared_mem, int cashier_id, int queue_id) {
    // Uzyskanie semafora
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return;
    }

    // Synchronizacja za pomocą semafora
    sem_wait(sem);  // Blokowanie dostępu do pamięci dzielonej

    // Sprawdzenie poprawności indeksu
    if (cashier_id > 0 && cashier_id <= MAX_CASHIERS) {
        shared_mem->queue_ids[cashier_id-1] = queue_id;
    } else {
        fprintf(stderr, "Błąd: invalid cashier_id: %d\n", cashier_id);
    }

    sem_post(sem);  // Zwolnienie semafora
}



int get_queue_id(SharedMemory* shared_mem, int cashier_id) {
    // Uzyskanie semafora
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return -1;
    }

    // Synchronizacja za pomocą semafora
    sem_wait(sem);  // Blokowanie dostępu do pamięci dzielonej
    int queue_id = shared_mem->queue_ids[cashier_id-1];
    sem_post(sem);  // Zwolnienie semafora

    return queue_id;
}



// Funkcja zwracająca wskaźnik do pamięci dzielonej
SharedMemory* get_shared_memory() {
    // Przyłączenie do istniejącej pamięci dzielonej
    int shm_id = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shm_id == -1) {
        perror("Błąd przyłączenia do pamięci dzielonej");
        exit(1);
    }

    SharedMemory* shared_mem = (SharedMemory*)shmat(shm_id, NULL, 0);
    if (shared_mem == (void*)-1) {
        perror("Błąd przypisania pamięci dzielonej");
        exit(1);
    }

    return shared_mem;
}


void cleanup_shared_memory(SharedMemory* shared_mem) {
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora do sprzątania");
        return;
    }
    // Synchronizacja za pomocą semafora
    sem_wait(sem);  // Blokowanie dostępu do pamięci dzielonej

    // Odłączenie pamięci dzielonej
    if (shmdt(shared_mem) == -1) {
        perror("Błąd odłączania pamięci dzielonej");
    }

    // Usunięcie segmentu pamięci dzielonej
    int shm_id = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shm_id != -1) {
        if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
            perror("Błąd usuwania pamięci dzielonej");
        } else {
            printf("Pamięć dzielona została poprawnie usunięta\n");
        }
    }

    sem_post(sem);  // Zwolnienie semafora
    // Czyszczenie semafora
    cleanup_semaphore();
}

sem_t* get_semaphore() {
    const char* semaphore_name = "/shared_mem_semaphore";
    
    // Uzyskanie semafora
    sem_t* sem = sem_open(semaphore_name, 0);  // Otwieranie istniejącego semafora
    if (sem == SEM_FAILED) {
        perror("Błąd otwierania semafora shared");
        return NULL;
    }

    return sem;
}


void cleanup_semaphore() {
    sem_t* sem = sem_open("/shared_mem_semaphore", 0);
    if (sem == SEM_FAILED) {
        perror("Błąd otwierania semafora do zamknięcia");
        return;
    }

    if (sem_close(sem) == -1) {
        perror("Błąd zamykania semafora");
    }

    if (sem_unlink("/shared_mem_semaphore") == -1) {
        perror("Błąd usuwania semafora");
    } else {
        printf("Semafor został poprawnie usunięty shared memory\n");
    }
}