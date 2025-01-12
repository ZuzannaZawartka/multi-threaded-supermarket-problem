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

int NUM_CUSTOMERS  = 30 ;// Liczba klientów (tymczasowo)


SharedMemory* shared_mem;
sem_t* shared_mem_semaphore; // Semafor dla synchronizacji dostępu do pamięci dzielonej


// Handler dla sygnału SIGINT (Ctrl+C)
void sigint_handler(int signum) {
    printf("Otrzymano sygnał SIGINT, kończenie pracy kasjerów...\n");

    // Zaktualizowanie flagi zakończenia
    set_should_exit(shared_mem, 1);

    // terminate_all_customers();

    printf("Wszystkie procesy i wątki zostaną zakończone.\n");
}

// Funkcja inicjalizująca semafor
void init_semaphore() {
    // Tworzymy semafor, jeśli jeszcze nie istnieje
    shared_mem_semaphore = sem_open("/shared_mem_semaphore", O_CREAT, 0666, 1);
    if (shared_mem_semaphore == SEM_FAILED) {
        perror("Błąd tworzenia semafora");
        exit(1);
    }
}

// Funkcja czyszcząca semafor
void cleanup_semaphore() {
    if (sem_close(shared_mem_semaphore) == -1) {
        perror("Błąd zamykania semafora");
    }
    if (sem_unlink("/shared_mem_semaphore") == -1) {
        perror("Błąd usuwania semafora");
    }
}


int main() {
    srand(time(NULL));  
    pthread_t monitor_thread; 

    // Inicjalizacja semafora
    init_semaphore();
    
    shared_mem = init_shared_memory(shared_mem_semaphore);  // Przypisz wskaźnik do pamięci dzielonej

    // Rejestracja handlera sygnału SIGINT
    signal(SIGINT, sigint_handler);

    // Tworzenie wątku dla managera kasjerow
    init_manager(&monitor_thread);


    // Tworzenie procesów klientów
    create_customer_processes(NUM_CUSTOMERS,DEFAULT_CASHIERS);


    // Czekanie na zakończenie procesów klientów
    wait_for_customers(NUM_CUSTOMERS);


    terminate_manager(monitor_thread);

    // Czyszczenie semafora
    cleanup_semaphore();

    //Czyszczenie pamięci dzielonej
    cleanup_shared_memory(shared_mem);

    return 0;
}



//dodac tego menadzera kasjerow ktory monitoruje liczbe ludzi w sklepie i na podstawie tego dodaje kasjerow

//dodac strazaka

