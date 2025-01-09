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
#include "manager_cashiers.h"


#define MAX_CASHIERS 10 // Maksymalna liczba kasjerów

#define DEFAULT_CASHIERS 2

int NUM_CUSTOMERS  = 30 ;// Liczba klientów (tymczasowo)

SharedMemory* shared_mem;

// Handler dla sygnału SIGINT (Ctrl+C)
void sigint_handler(int signum) {
    printf("Otrzymano sygnał SIGINT, kończenie pracy kasjerów...\n");

    // Zaktualizowanie flagi zakończenia
    set_should_exit(shared_mem, 1);

    // terminate_all_customers();

    printf("Wszystkie procesy i wątki zostaną zakończone.\n");
}


int main() {
    srand(time(NULL));  

    pthread_t monitor_thread; 
    // pthread_t cashier_threads[MAX_CASHIERS];
    // int cashier_ids[MAX_CASHIERS];
     
    shared_mem = init_shared_memory();  // Przypisz wskaźnik do pamięci dzielonej

    // Rejestracja handlera sygnału SIGINT
    signal(SIGINT, sigint_handler);

    // Tworzenie wątku dla managera kasjerow
    init_manager(&monitor_thread);

    // Tworzenie kasjerów
    // init_cashiers(cashier_threads, cashier_ids, DEFAULT_CASHIERS);

    // Tworzenie procesów klientów
    create_customer_processes(NUM_CUSTOMERS, DEFAULT_CASHIERS);

    // Czekanie na zakończenie procesów klientów
    wait_for_customers(NUM_CUSTOMERS);

    // Czekanie na zakończenie kasjerów
    // wait_for_cashiers(cashier_threads, DEFAULT_CASHIERS);

    terminate_manager(monitor_thread);

    //Czyszczenie pamięci dzielonej
    cleanup_shared_memory(shared_mem);

    return 0;
}



//dodac tego menadzera kasjerow ktory monitoruje liczbe ludzi w sklepie i na podstawie tego dodaje kasjerow

//dodac strazaka

