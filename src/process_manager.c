#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>  // Dla semaforów POSIX
#include <sys/stat.h>
#include <errno.h>
#include "process_manager.h"

#define SEM_NAME "/process_list_semaphore" //nazwa semafora dla listy procesów
ProcessNode* process_list = NULL;  // Lista powiązań przechowująca pidy procesów

void init_semaphore_process() {
    sem_t* process_list_semaphore = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0600, 1);
    if (process_list_semaphore == SEM_FAILED) {
        if (errno == EEXIST) {
            // Semafor już istnieje - otwieramy go
            process_list_semaphore = sem_open(SEM_NAME, 0);
            if (process_list_semaphore == SEM_FAILED) {
                perror("Błąd otwierania istniejącego semafora");
                exit(1);
            }
        } else {
            perror("Błąd tworzenia semafora");
            exit(1);
        }
    } else {
        printf("Semafor procesów utworzony\n");
    }

    // Zwolnij semafor na koniec inicjalizacji (ustawienie początkowej wartości)
    if (sem_post(process_list_semaphore) == -1) {
        perror("Błąd zwolnienia semafora");
    }

    if (sem_close(process_list_semaphore) == -1) {
        perror("Błąd zamykania semafora po inicjalizacji");
    }
}

// Funkcja czyszcząca semafor
void cleanup_semaphore_process() {
    sem_t* sem = get_semaphore_process();
    if (sem_close(sem) == -1) {
        perror("Błąd zamykania semafora");
    }
    if (sem_unlink(SEM_NAME) == -1) {
        perror("Błąd usuwania semafora");
    }

    printf("Semafor procesów klienta został poprawnie usunięty\n");
}

// Funkcja uzyskująca semafor istniejący (bez tworzenia nowego)
sem_t* get_semaphore_process() {
    sem_t* sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("Błąd otwierania semafora process_list_semaphore");
        return NULL;
    }
    return sem;
}


// Dodaj proces do listy
void add_process(pid_t pid) {
    // Uzyskujemy semafor przed manipulowaniem listą
    sem_t* sem = get_semaphore_process();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return;
    }

    // Synchronizacja za pomocą semafora
    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return;
    }

    if (process_exists(pid)) {
        printf("PID %d już istnieje w liście\n", pid);
        sem_post(sem);  // Zwolnienie semafora
        return;
    }

    ProcessNode* new_node = (ProcessNode*)malloc(sizeof(ProcessNode));
    if (new_node == NULL) {
        perror("Błąd alokacji pamięci");
        sem_post(sem);  // Zwolnienie semafora
        exit(1);
    }

    new_node->pid = pid;
    new_node->next = process_list;
    process_list = new_node;

    // Zwolnienie semafora po zakończeniu operacji na liście
    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
    }
}

// Usuń proces z listy
void remove_process(pid_t pid) {
    // Uzyskujemy semafor przed manipulowaniem listą
    sem_t* sem = get_semaphore_process();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return;
    }

    // Synchronizacja za pomocą semafora
    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return;
    }

    ProcessNode* current = process_list;
    ProcessNode* prev = NULL;
    
    // Szukamy PID-a w liście
    while (current != NULL && current->pid != pid) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        sem_post(sem);  // Zwolnienie semafora
        return; // PID nie znaleziono w liście
    }

    if (prev == NULL) {
        process_list = current->next;  
    } else {
        prev->next = current->next; 
    }

    free(current); 

    // Zwolnienie semafora po zakończeniu operacji na liście
    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
    }
}

// Sprawdź, czy proces o podanym PID istnieje w liście
int process_exists(pid_t pid) {
    // Uzyskujemy semafor przed odczytem listy
    sem_t* sem = get_semaphore_process();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return 0;
    }

    // Synchronizacja za pomocą semafora
    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return 0;
    }

    ProcessNode* current = process_list;
    while (current != NULL) {
        if (current->pid == pid) {
            sem_post(sem);  // Zwolnienie semafora
            return 1; 
        }
        current = current->next;
    }

    sem_post(sem);  // Zwolnienie semafora
    return 0; 
}

// Czyszczenie wszystkich procesów w liście
void cleanup_processes() {
    // Uzyskujemy semafor przed manipulowaniem listą
    sem_t* sem = get_semaphore_process();
    if (sem == NULL) {
        perror("Błąd uzyskiwania semafora");
        return;
    }

    // Synchronizacja za pomocą semafora
    if (sem_wait(sem) == -1) {
        perror("Błąd oczekiwania na semafor");
        return;
    }

    ProcessNode* current = process_list;

    while (current != NULL) {
        ProcessNode* next = current->next; 
        free(current); 
        current = next;
    }

    process_list = NULL;  

    // Zwolnienie semafora po zakończeniu operacji na liście
    if (sem_post(sem) == -1) {
        perror("Błąd zwolnienia semafora");
    }
}
