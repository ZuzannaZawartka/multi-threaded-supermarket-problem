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

#include <sys/sem.h>  // Do operacji na semaforach systemowych
#include <sys/ipc.h> 
#include <fcntl.h>  // Dla O_CREAT

#define MAX_CASHIERS 10 // Maksymalna liczba kasjerów

#define DEFAULT_CASHIERS 2

int NUM_CUSTOMERS  = 5 ;// Liczba klientów (tymczasowo)

SharedMemory* shared_mem;
pthread_t monitor_thread;  // Declare it as a global variable
pthread_t customer_thread;

void send_signal_to_manager(int signal) {
    if (pthread_kill(monitor_thread, signal) != 0) {
            perror("Błąd wysyłania sygnału do kasjera");
    }
}

void send_signal_to_customers(int signal) {
    if (pthread_kill(customer_thread, signal) != 0) {
            perror("Błąd wysyłania sygnału do kasjera");
    }
}

void mainHandlerProcess(int signum) {
    send_signal_to_manager(SIGTERM);  // Wysyłanie sygnału do wątku menedżera
    send_signal_to_customers(SIGUSR2);  // Wysyłanie sygnału do wątku menedżera
}

int main() {
    srand(time(NULL));  

    signal(SIGINT, mainHandlerProcess);// Rejestracja handlera SIGINT
    shared_mem = init_shared_memory();
    // Tworzenie wątku dla managera kasjerow
    init_manager(&monitor_thread);

    // Tworzenie wątku dla klientów
    if (pthread_create(&customer_thread, NULL, create_customer_processes, NULL) != 0) {
        perror("Błąd tworzenia wątku dla klientów");
        exit(1);
    }

    // Czekanie na zakończenie procesów klientów
    wait_for_customers();

    terminate_manager(monitor_thread);

    //Czyszczenie pamięci dzielonej
    cleanup_shared_memory(shared_mem);

    printf("KONIEC");
    return 0;
}

