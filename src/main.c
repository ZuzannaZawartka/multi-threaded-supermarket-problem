#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>     
#include "cashier.h"
#include "customer.h"
#include "shared_memory.h"


#define MAX_CASHIERS 10 // Maksymalna liczba kasjerów

#define DEFAULT_CASHIERS 2

#define NUM_CUSTOMERS 15 // Liczba klientów (tymczasowo)

SharedMemory* shared_mem;

// Handler dla sygnału SIGINT (Ctrl+C)
void sigint_handler(int signum) {
    printf("Otrzymano sygnał SIGINT, kończenie pracy kasjerów...\n");

    // Zaktualizowanie flagi zakończenia
    set_should_exit(shared_mem, 1);

    terminate_all_customers();

    printf("Wszystkie procesy i wątki zostaną zakończone.\n");
}



int main() {
    pthread_t cashier_threads[MAX_CASHIERS];
    int cashier_ids[MAX_CASHIERS];

    shared_mem = init_shared_memory();  // Przypisz wskaźnik do pamięci dzielonej

    // Rejestracja handlera sygnału SIGINT
    signal(SIGINT, sigint_handler);

    // Tworzenie kasjerów
    init_cashiers(cashier_threads, cashier_ids, DEFAULT_CASHIERS);

    // Tworzenie procesów klientów
    create_customer_processes(NUM_CUSTOMERS, DEFAULT_CASHIERS);

    // Czekanie na zakończenie procesów klientów
    wait_for_customers(NUM_CUSTOMERS);

    // Czekanie na zakończenie kasjerów
    wait_for_cashiers(cashier_threads, DEFAULT_CASHIERS);

    //Czyszczenie pamięci dzielonej
    cleanup_shared_memory(shared_mem);

    return 0;
}
