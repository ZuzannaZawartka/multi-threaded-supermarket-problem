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
extern int should_exit=0;

// Handler dla sygnału SIGINT (Ctrl+C)
void sigint_handler(int signum) {
    printf("Otrzymano sygnał SIGINT, kończenie pracy kasjerów...\n");
    
    // Ustawienie flagi zakończenia
    should_exit = 1;
    
    terminate_all_customers();

    // Kasjerzy zakończą swoje wątki po sygnale
    printf("Wszystkie procesy i wątki zostaną zakończone.\n");
}


int main() {
    pthread_t cashier_threads[NUM_CASHIERS];
    int cashier_ids[NUM_CASHIERS];

    // Rejestracja handlera sygnału SIGINT
    signal(SIGINT, sigint_handler);

    // Tworzenie kasjerów
    init_cashiers(cashier_threads, cashier_ids, NUM_CASHIERS);

    // Tworzenie procesów klientów
    create_customer_processes(NUM_CUSTOMERS, NUM_CASHIERS);

    // Czekanie na zakończenie procesów klientów
    wait_for_customers(NUM_CUSTOMERS);

    // Czekanie na zakończenie kasjerów
    wait_for_cashiers(cashier_threads, NUM_CASHIERS);

    return 0;
}
