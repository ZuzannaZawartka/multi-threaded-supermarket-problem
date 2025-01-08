// shared_memory.c
#include <sys/shm.h>
#include <sys/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "shared_memory.h"  // Załączanie nagłówka, który zawiera SharedMemory

#define SHM_KEY 12345  // Klucz do pamięci dzielonej

SharedMemory* init_shared_memory() {
    // Tworzenie pamięci dzielonej
    int shm_id = shmget(SHM_KEY, sizeof(SharedMemory), 0666 | IPC_CREAT);
    if (shm_id == -1) {
        perror("Błąd tworzenia pamięci dzielonej");
        exit(1);
    }

    // Mapowanie pamięci dzielonej do przestrzeni procesu
    SharedMemory* shared_mem = (SharedMemory*)shmat(shm_id, NULL, 0);
    if (shared_mem == (void*)-1) {
        perror("Błąd przypisania pamięci dzielonej");
        exit(1);
    }

    // Inicjalizowanie zmiennej should_exit i mutexu
    shared_mem->should_exit = 0;
    if (pthread_mutex_init(&shared_mem->mutex, NULL) != 0) {
        perror("Błąd inicjalizacji mutexu");
        exit(1);
    }

    return shared_mem;  // Funkcja powinna zawsze zwracać wskaźnik do pamięci dzielonej
}

void cleanup_shared_memory(SharedMemory* shared_mem) {
    // Odłączenie pamięci dzielonej
    if (shmdt(shared_mem) == -1) {
        perror("Błądd odłączania pamięci dzielonej");
    }

    // Usunięcie pamięci dzielonej tylko jeżeli segment istnieje
    int shm_id = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shm_id != -1) {
        if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
            perror("Błąd usuwania pamięci dzielonej");
        } else {
            printf("Pamięć dzielona usuniętta\n");
        }
    } else {
        printf("Segment pamięci dzielonej nie istnieje,lub został już usuniety.\n");
    }
}

// Funkcja do ustawienia flagi zakończenia w pamięci dzielonej
void set_should_exit(SharedMemory* shared_mem, int value) {
    
    // Zabezpieczenie dostępu do flagi przy pomocy mutexu
    pthread_mutex_lock(&shared_mem->mutex);
    shared_mem->should_exit = value;
    pthread_mutex_unlock(&shared_mem->mutex);
}

// Funkcja do odczytu flagi zakończenia
int get_should_exit(SharedMemory* shared_mem) {
    // Zabezpieczenie dostępu do flagi przy pomocy mutexu
    pthread_mutex_lock(&shared_mem->mutex);
    int value = shared_mem->should_exit;
    pthread_mutex_unlock(&shared_mem->mutex);
    return value;
}
