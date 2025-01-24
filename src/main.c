#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>   
#include <errno.h>   
#include "creator_customer.h"
#include "shared_memory.h"
#include "manager_cashiers.h"
#include "firefighter.h"


SharedMemory* shared_mem = NULL;
pthread_t monitor_thread;  // Declare it as a global variable
pthread_t customer_thread;
pthread_t firefighter_thread; // Wątek strażaka

// Mutex i zmienna warunkowa
pthread_mutex_t customers_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t customers_cond = PTHREAD_COND_INITIALIZER;

int main() {
    srand(time(NULL));  
    init_semaphore_customer();
    shared_mem = init_shared_memory();
    signal(SIGINT, mainHandlerProcess);// Rejestracja handlera SIGINT dla pożaru

    // inicjalizacja semafora zliczającego klientów
    // init_firefighter(&firefighter_thread);// tworzenie wątku dla strażaka
    init_manager(&monitor_thread);  // Tworzenie wątku dla managera kasjerow

    if (pthread_create(&customer_thread, NULL, create_customer_processes, NULL) != 0) { // Tworzenie wątku który tworzy klientów
        perror("Błąd tworzenia wątku dla klientów");
        exit(1);
    }

    wait_for_customers(); //Czekanie na zakończenie procesów klientów
    printf("PRZEJSCIE DO MANAGERA \n");
    wait_for_manager(monitor_thread); //usuniecie manadżera
    // // wait_for_firefighter(firefighter_thread);//usunięcie strażaka

    cleanup_shared_memory(shared_mem); //czyszczenie pamięci dzielonej i jej semafora
    destroy_semaphore_customer();
    printf("KONIEC\n");
    return 0;
}

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

void send_signal_to_firefighter(int signal) {
    if (pthread_kill(firefighter_thread, signal) != 0) {
        perror("Błąd wysyłania sygnału do strażaka"); //(TOFIX ?)tutaj blad wyrzuci gdy strazak sam sie wywola
    }
}

void mainHandlerProcess(int signum) {
        // Wyślij sygnał do całej grupy procesów


    send_signal_to_customers(SIGUSR2);  // Wysyłanie sygnału do klientów

    send_signal_to_manager(SIGTERM);  // Wysyłanie sygnału do wątku menedżera

    // send_signal_to_firefighter(SIGQUIT); //wysylanie sygnału do strażaka
    countdown_to_exit();
}

void set_process_group() {
    pid_t pid = getpid();
     if (setpgid(pid, pid) == -1) {// Ustawienie głównego procesu jako lidera grupy procesów (po to żeby strażak mógł wysłać poprawnie sygnały)
        perror("Błąd przy ustawianiu grupy procesów");
        exit(1);  // Zakończenie programu w przypadku błędu
    } 
}
