#include <sys/shm.h>
#include <sys/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>  // Załączamy semafor
#include <sys/msg.h>  // Nagłówek do obsługi kolejek komunikatów
#include <fcntl.h>
#include "shared_memory.h"

#define SHM_KEY 12387 
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

    //Zwolnienie semafora
    if (sem_post(shared_mem_semaphore) == -1) {
        perror("Błąd zwolnienia semafora");
        exit(1);
    }

    // Uzyskujemy semafor przed dostępem do pamięci dzielonej
    if (sem_wait(shared_mem_semaphore) == -1) {
        perror("Błąd oczekiwania na semafor");
        exit(1);
    }

    for (int i = 0; i < MAX_CASHIERS; i++) {
        shared_mem->queue_ids[i] = -1;  // Wartość -1 oznacza brak kolejki (inicjalizacja tablicy kolejek quques)
    }

    shared_mem->active_cashiers = 0; //ustawienie aktywnych kasjerów na 0
    if (sem_post(shared_mem_semaphore) == -1) {
        perror("Błąd zwolnienia semafora");
        exit(1);
    }
    return shared_mem;
}

//ustawienie kolejki komunikatów dla określonego kasjera
void set_queue_id(SharedMemory* shared_mem, int cashier_id, int queue_id) {
    // Uzyskanie semafora
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return;
    }

    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        exit(1);
    }

    // Sprawdzenie poprawności indeksu
    if (cashier_id > 0 && cashier_id <= MAX_CASHIERS) {//poniewz kasjerzy od 1 do 10
        shared_mem->queue_ids[cashier_id-1] = queue_id;
    } else {
        fprintf(stderr, "Błąd: invalid cashier_id: %d\n", cashier_id);
    }

    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
        exit(1);
    }
}

int get_queue_id(SharedMemory* shared_mem, int cashier_id) {
    // Uzyskanie semafora
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return -1;
    }

    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        exit(1);
    }
    int queue_id = shared_mem->queue_ids[cashier_id-1];
    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
        exit(1);
    }

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

    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return;
    }

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

    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
    }

    // Czyszczenie semafora
    cleanup_semaphore();
}

sem_t* get_semaphore() {
    // Uzyskanie semafora
    sem_t* sem = sem_open(SEM_NAME, 0);  // Otwieranie istniejącego semafora
    if (sem == SEM_FAILED) {
        perror("Błąd otwierania semafora shared");
        return NULL;
    }
    return sem;
}

//usuwanie semafora dla shared memory
void cleanup_semaphore() {
    sem_t* sem = get_semaphore();

    if (sem_close(sem) == -1) {
        perror("Błąd zamykania semafora");
    }

    if (sem_unlink(SEM_NAME) == -1) {
        perror("Błąd usuwania semafora");
    } else {
        printf("Semafor został poprawnie usunięty shared memory\n");
    }
}

//zwracanie ilosc aktywnych kasjerów do których można się ustawić w kolejce
int get_active_cashiers(SharedMemory* shared_mem) {
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return -1;
    }

    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return -1;
    }

    // Odczytanie liczby kasjerów
    int active_cashiers = shared_mem->active_cashiers;

    // Zwolnienie semafora
    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
    }
    return active_cashiers;
}

//zwiększanie liczby aktywnych kasjerów
void increment_active_cashiers(SharedMemory* shared_mem) {
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return;
    }

    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return;
    }

    // Zwiększenie liczby kasjerów, jeśli nie przekracza maksymalnej liczby
    if (shared_mem->active_cashiers <= MAX_CASHIERS) {
        shared_mem->active_cashiers++;
    } else {
        fprintf(stderr, "Błąd: liczba kasjerów nie może przekroczyć %d\n", MAX_CASHIERS);
    }
 
    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
    }
}

//zmienjszanie liczby aktywnych kasjerów
void decrement_active_cashiers(SharedMemory* shared_mem) {
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return;
    }

    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return;
    }

    // Zmniejszenie liczby kasjerów, jeśli jest większa niż 0
    if (shared_mem->active_cashiers > 0) {
        shared_mem->active_cashiers--;
    } else {
        fprintf(stderr, "Błąd: liczba kasjerów nie może być mniejsza niż 0\n");
    }

    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
    }
}
