#include <sys/shm.h>
#include <sys/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/msg.h>
#include <fcntl.h>
#include "shared_memory.h"

#define SHM_KEY 13387 
#define SEM_NAME "/shared_mem_semaphore" 

/**
 * @brief Inicjalizuje pamięć dzieloną i semafor do niej przypisany.
 * 
 * @details Funkcja tworzy segment pamięci dzielonej i semafor POSIX do jej synchronizacji. 
 * Inicjalizuje pola pamięci dzielonej, w tym tablicę kolejek oraz zmienne sterujące.
 * 
 * @return Wskaźnik do zainicjalizowanej pamięci dzielonej.
 */
SharedMemory* init_shared_memory() {

    // Tworzenie pamięci dzielonej
    int shm_id = shmget(SHM_KEY, sizeof(SharedMemory), 0600 | IPC_CREAT);
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
    sem_t* shared_mem_semaphore = sem_open(SEM_NAME, O_CREAT, 0600, 1);
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
        shared_mem->queue_ids[i] = -1;          // Wartość -1 oznacza brak kolejki (inicjalizacja tablicy kolejek quques)
    }

    shared_mem->active_cashiers = 0;            //ustawienie aktywnych kasjerów na 0
    shared_mem->customer_count = 0;             //ustawienie aktywnych kasjerów na 0
    shared_mem->fire_handling_complete = 0;     //ustawieni flagi informującej o końcu tworzenia klientów

    if (sem_post(shared_mem_semaphore) == -1) {
        perror("Błąd zwolnienia semafora");
        exit(1);
    }

    printf("Pamięć Dzielona Zainicjalizowana\n");

    return shared_mem;
}

/**
 * @brief Ustawia identyfikator kolejki komunikatów dla danego kasjera.
 * 
 * @param shared_mem Wskaźnik do pamięci dzielonej.
 * @param cashier_id Identyfikator kasjera (1-10).
 * @param queue_id Identyfikator kolejki komunikatów.
 */
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

/**
 * @brief Pobiera identyfikator kolejki komunikatów dla danego kasjera.
 * 
 * @param shared_mem Wskaźnik do pamięci dzielonej.
 * @param cashier_id Identyfikator kasjera (1-10).
 * 
 * @return Identyfikator kolejki komunikatów lub -1 w przypadku błędu.
 */
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

/**
 * @brief Zwraca wskaźnik do już istniejącej pamięci dzielonej.
 * 
 * @return Wskaźnik do pamięci dzielonej.
 */
SharedMemory* get_shared_memory() {
    // Przyłączenie do istniejącej pamięci dzielonej
    int shm_id = shmget(SHM_KEY, sizeof(SharedMemory), 0600);
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

/**
 * @brief Czyści pamięć dzieloną i powiązany semafor.
 * 
 * @param shared_mem Wskaźnik do pamięci dzielonej.
 */
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
    int shm_id = shmget(SHM_KEY, sizeof(SharedMemory), 0600);
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

    cleanup_semaphore();
}

/**
 * @brief Pobiera wskaźnik do istniejącego semafora powiązanego z pamięcią dzieloną.
 * 
 * @return Wskaźnik do semafora lub NULL w przypadku błędu.
 */
sem_t* get_semaphore() {
    // Uzyskanie semafora
    sem_t* sem = sem_open(SEM_NAME, 0);  // Otwieranie istniejącego semafora
    if (sem == SEM_FAILED) {
        perror("Błąd otwierania semafora shared");
        return NULL;
    }
    return sem;
}

/**
 * @brief Usuwa semafor powiązany z pamięcią dzieloną.
 */
void cleanup_semaphore() {
    sem_t* sem = get_semaphore();

    if (sem_close(sem) == -1) {
        perror("Błąd zamykania semafora");
        return;
    }

    if (sem_unlink(SEM_NAME) == -1) {
        perror("Błąd usuwania semafora");
        return;
    } else {
        printf("Semafor został poprawnie usunięty shared memory\n");
    }
}

/**
 * @brief Zwraca liczbę aktywnych kasjerów.
 * 
 * @param shared_mem Wskaźnik do pamięci dzielonej.
 * 
 * @return Liczba aktywnych kasjerów.
 */
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
        return -1;
    }
    return active_cashiers;
}

/**
 * @brief Zwiększa liczbę aktywnych kasjerów w pamięci dzielonej.
 * 
 * @param shared_mem Wskaźnik do pamięci dzielonej.
 * 
 * @details Funkcja zwiększa licznik aktywnych kasjerów o jeden, jeśli liczba aktywnych kasjerów
 * nie przekracza wartości `MAX_CASHIERS`. Operacja jest chroniona semaforem.
 */
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
        return;
    }
}

/**
 * @brief Zmniejsza liczbę aktywnych kasjerów w pamięci dzielonej.
 * 
 * @param shared_mem Wskaźnik do pamięci dzielonej.
 * 
 * @details Funkcja zmniejsza licznik aktywnych kasjerów o jeden, jeśli liczba aktywnych kasjerów
 * jest większa niż 0. Operacja jest chroniona semaforem.
 */
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
        return;
    }
}

/**
 * @brief Zwiększa licznik klientów w pamięci dzielonej.
 * 
 * @param shared_mem Wskaźnik do pamięci dzielonej.
 * 
 * @details Funkcja zwiększa licznik klientów o jeden. Operacja jest chroniona semaforem.
 */
void increment_customer_count(SharedMemory* shared_mem) {
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return;
    }

    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return;
    }
    shared_mem->customer_count+=1;

    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
        return;
    }
}

/**
 * @brief Zmniejsza licznik klientów w pamięci dzielonej.
 * 
 * @param shared_mem Wskaźnik do pamięci dzielonej.
 * 
 * @details Funkcja zmniejsza licznik klientów o jeden, jeśli liczba klientów jest większa niż 0.
 * Operacja jest chroniona semaforem.
 */
void decrement_customer_count(SharedMemory* shared_mem) {
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return;
    }
    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return;
    }
    shared_mem->customer_count--;
    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
        return;
    }

}

/**
 * @brief Zwraca liczbę aktualnych klientów.
 * 
 * @param shared_mem Wskaźnik do pamięci dzielonej.
 * 
 * @return Liczba klientów w pamięci dzielonej.
 * 
 * @details Funkcja zwraca wartość licznika klientów. Operacja jest chroniona semaforem.
 */
int get_customer_count(SharedMemory* shared_mem) {
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return -1;
    }

    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return -1;
    }

    int customer_count = shared_mem->customer_count;

    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
        return -1;
    }
    return customer_count;
}

/**
 * @brief Ustawia flagę pożaru w pamięci dzielonej.
 * 
 * @param shared_mem Wskaźnik do pamięci dzielonej.
 * @param fire_status Wartość flagi (`1` oznacza pożar, `0` brak pożaru).
 * 
 * @details Funkcja ustawia flagę pożaru w pamięci dzielonej. Operacja jest chroniona semaforem.
 */
void set_fire_flag(SharedMemory* shared_mem, int fire_status) {
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return;
    }

    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return;
    }

    shared_mem->fire_flag = fire_status;

    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
    }
}

/**
 * @brief Pobiera wartość flagi pożaru z pamięci dzielonej.
 * 
 * @param shared_mem Wskaźnik do pamięci dzielonej.
 * 
 * @return Wartość flagi pożaru (`1` oznacza pożar, `0` brak pożaru).
 * 
 * @details Funkcja zwraca aktualną wartość flagi pożaru z pamięci dzielonej.
 * Operacja jest chroniona semaforem.
 */
int get_fire_flag(SharedMemory* shared_mem) {
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return -1;
    }

    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return -1;
    }

    int fire_status = shared_mem->fire_flag;

    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
    }

    return fire_status;
}

/**
 * @brief Ustawia flagę informującą o zakończeniu obsługi pożaru.
 * 
 * @param shared_mem Wskaźnik do pamięci dzielonej.
 * @param status Wartość flagi (`1` oznacza zakończenie obsługi, `0` brak zakończenia obsługi).
 * 
 * @details Funkcja ustawia flagę `fire_handling_complete` w pamięci dzielonej.
 * Operacja jest chroniona semaforem.
 */
void set_fire_handling_complete(SharedMemory* shared_mem, int status) {
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return;
    }

    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return;
    }

    shared_mem->fire_handling_complete = status;

    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
    }
}

/**
 * @brief Pobiera wartość flagi informującej o zakończeniu obsługi pożaru.
 * 
 * @param shared_mem Wskaźnik do pamięci dzielonej.
 * 
 * @return Wartość flagi `fire_handling_complete` (`1` oznacza zakończenie obsługi, `0` brak zakończenia obsługi).
 * 
 * @details Funkcja zwraca aktualną wartość flagi `fire_handling_complete`.
 * Operacja jest chroniona semaforem.
 */
int get_fire_handling_complete(SharedMemory* shared_mem) {
    sem_t* sem = get_semaphore();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return -1;
    }

    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return -1;
    }

    int status = shared_mem->fire_handling_complete;

    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
    }

    return status;
}
