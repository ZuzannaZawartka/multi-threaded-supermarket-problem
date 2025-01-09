#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "manager_cashiers.h"
#include "shared_memory.h"

extern SharedMemory* shared_mem;  // Deklaracja pamięci dzielonej

// Funkcja menedżera kasjerów - monitorowanie liczby klientów
void* manage_customers(void* arg) {
    while (!get_should_exit(shared_mem)) {
        // Pobranie aktualnej liczby klientów z pamięci dzielonej
        sem_wait(&shared_mem->semaphore);
        int current_customers = shared_mem->number_customer;
        sem_post(&shared_mem->semaphore);

        // Wyświetlenie liczby klientów co sekundę
        printf("Aktualna liczba klientów w sklepie: %d\n", current_customers);

        sleep(1);  // Czekaj przez 1 sekundę, zanim wyświetlisz liczbę ponownie
    }

    printf("Menadżer zakończył działanie.\n");
    return NULL;
}

// Funkcja do inicjalizacji menedżera
void init_manager(pthread_t* manager_thread) {
    // Tworzenie wątku menedżera kasjerów, który będzie monitorował liczbę klientów
    if (pthread_create(manager_thread, NULL, manage_customers, NULL) != 0) {
        perror("Błąd tworzenia wątku menedżera kasjerów");
        exit(1);
    }
}

// Funkcja do zakończenia wątku menedżera
void terminate_manager(pthread_t manager_thread) {
    // Ustawienie flagi `should_exit` w pamięci dzielonej, aby zakończyć działanie menedżera
    set_should_exit(shared_mem, 1);

    // Czekanie na zakończenie wątku menedżera
    void* status;
    int ret = pthread_join(manager_thread, &status);  // Czeka na zakończenie wątku
    if (ret == 0) {
        printf("Wątek menedżera zakończył się pomyślnie.\n");
    } else {
        printf("Błąd podczas oczekiwania na zakończenie wątku menedżera.\n");
    }
}
