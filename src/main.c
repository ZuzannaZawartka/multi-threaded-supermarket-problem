#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>     
#include "cashier.h"
#include "customer.h"

#define NUM_CASHIERS 3
#define NUM_CUSTOMERS 5

// Zmienna globalna do sygnalizowania zakończenia pracy
extern int should_exit;

// Tablica przechowująca PID procesów klientów (tymczasowo) narazie aby usuwac procesy klientów.
pid_t customer_pids[NUM_CUSTOMERS];

// Handler dla sygnału SIGINT (Ctrl+C)
void sigint_handler(int signum) {
    printf("Otrzymano sygnał SIGINT, kończenie pracy kasjerów...\n");
    
    // Ustawienie flagi zakończenia
    should_exit = 1;
    
    // Zabijanie wszystkich procesów klientów
    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        if (customer_pids[i] > 0) {
            kill(customer_pids[i], SIGTERM);  // Wysyłanie sygnału zakończenia do klienta
        }
    }

    // Kasjerzy zakończą swoje wątki po sygnale
    printf("Wszystkie procesy i wątki zostaną zakończone.\n");
}

// Tworzenie procesów klientów
void create_customer_process(int customer_id, int cashier_id) {
    pid_t pid = fork();
    
    if (pid == 0) { 
        customer_function(&cashier_id);  // Klient działa, przypisany do danego kasjera
        exit(0); // Zakończenie procesu klienta
    } else if (pid < 0) {
        perror("Błąd tworzenia procesu klienta");
        exit(1);
    }

    // Zapisz PID klienta
    customer_pids[customer_id] = pid;
}

// Funkcja czekająca na zakończenie procesów klientów
void wait_for_customers(int num_customers) {
    for (int i = 0; i < num_customers; i++) {
        wait(NULL);  // Czeka na zakończenie każdego procesu klienta
    }
}

// Funkcja do oczekiwania na zakończenie pracy kasjerów
void wait_for_cashiers(pthread_t* cashier_threads, int num_cashiers) {
    for (int i = 0; i < num_cashiers; i++) {
        pthread_join(cashier_threads[i], NULL);  // Czeka na zakończenie każdego wątku kasjera
    }
}

int main() {
    pthread_t cashier_threads[NUM_CASHIERS];
    int cashier_ids[NUM_CASHIERS];
    
    // Rejestracja handlera sygnału SIGINT
    signal(SIGINT, sigint_handler);

    // Tworzenie kasjerów
    init_cashiers(cashier_threads, cashier_ids, NUM_CASHIERS);

    // Tworzenie procesów klientów
    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        int customer_cashier_id = rand() % NUM_CASHIERS + 1;  // Losowy kasjer dla klienta
        create_customer_process(i, customer_cashier_id);  // Tworzenie procesu klienta
    }

    // Czekanie na zakończenie procesów klientów
    wait_for_customers(NUM_CUSTOMERS);

    // Czekanie na zakończenie kasjerów
    wait_for_cashiers(cashier_threads, NUM_CASHIERS);

    return 0;
}
