#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "manager_cashiers.h"
#include "cashier.h"
#include "shared_memory.h"

#define MIN_PEOPLE_FOR_CASHIER 5  // Na każdą grupę 5 klientów przypada jeden kasjer
#define MAX_CASHIERS 10 
#define MIN_CASHIERS 2

extern SharedMemory* shared_mem;  // Deklaracja pamięci dzielonej

void* manage_customers(void* arg) {

    pthread_t cashier_threads[MAX_CASHIERS];  // Tablica do przechowywania wątków kasjerów
    int cashier_ids[MAX_CASHIERS];
    int current_cashiers = 0; 

    create_initial_cashiers(cashier_threads,cashier_ids,&current_cashiers);

    while (!get_should_exit(shared_mem)) {
        // Pobranie aktualnej liczby klientów z pamięci dzielonej
        int current_customers =  get_number_customer(shared_mem);

        // Wyświetlenie liczby klientów co sekundę
        printf("Aktualna liczba klientów w sklepie: %d\n", current_customers);


        // Tworzenie kasjerów na podstawie liczby klientów
        if (current_customers >= MIN_PEOPLE_FOR_CASHIER * (current_cashiers + 1) && current_cashiers < MAX_CASHIERS) {
            // Tworzymy nowego kasjera
            cashier_ids[current_cashiers] = current_cashiers + 1;  // Identyfikator kasjera (od jedynki)
            create_cashier(&cashier_threads[current_cashiers], &cashier_ids[current_cashiers]);
            current_cashiers++;  // Zwiększamy liczbę kasjerów
        }

        sleep(1);  // Czekaj przez 1 sekundę, zanim wyświetlisz liczbę ponownie
    }


    wait_for_cashiers(cashier_threads, current_cashiers);  
    printf("Menadżer zakończył działanie.\n");
    return NULL;
}

// Funkcja do tworzenia kasjerów o poczatkowej liczbie
void create_initial_cashiers(pthread_t* cashier_threads, int* cashier_ids, int* current_cashiers) {
    for (int i = 0; i < MIN_CASHIERS; i++) {
        cashier_ids[i] = i + 1;  // Identyfikator kasjera (liczony od 1)
        create_cashier(&cashier_threads[i], &cashier_ids[i]);
        (*current_cashiers)++;  // Zwiększamy liczbę kasjerów
    }
    printf("Utworzono minimalną liczbę kasjerów: %d\n", MIN_CASHIERS);
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
    // Czekanie na zakończenie wątku menedżera
    void* status;
    int ret = pthread_join(manager_thread, &status);  // Czeka na zakończenie wątku
    if (ret == 0) {
        // printf("Wątek menedżera zakończył się pomyślnie.\n");
    } else {
        printf("Błąd podczas oczekiwania na zakończenie wątku menedżera.\n");
    }
}
