#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>   
#include <errno.h>   
#include "customer.h"
#include "shared_memory.h"
#include "manager_cashiers.h"
#include "firefighter.h"
#include "process_manager.h"


SharedMemory* shared_mem;
pthread_t monitor_thread;  // Declare it as a global variable
pthread_t customer_thread;
pthread_t firefighter_thread; // Wątek strażaka

int main() {
    srand(time(NULL));  

    signal(SIGINT, mainHandlerProcess);// Rejestracja handlera SIGINT dla pożaru
    shared_mem = init_shared_memory();
    init_manager(&monitor_thread);  // Tworzenie wątku dla managera kasjerow
    init_firefighter(&firefighter_thread);// tworzenie wątku dla strażaka

    if (pthread_create(&customer_thread, NULL, create_customer_processes, NULL) != 0) { // Tworzenie wątku który tworzy klientów
        perror("Błąd tworzenia wątku dla klientów");
        exit(1);
    }

    wait_for_customers(); //Czekanie na zakończenie procesów klientów
    wait_for_manager(monitor_thread); //usuniecie manadżera
    wait_for_firefighter(firefighter_thread);//usunięcie strażaka

    cleanup_shared_memory(shared_mem); //czyszczenie pamięci dzielonej i jej semafora
    cleanup_semaphore_process();
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
    send_signal_to_manager(SIGTERM);  // Wysyłanie sygnału do wątku menedżera
    send_signal_to_customers(SIGUSR2);  // Wysyłanie sygnału do klientów
    send_signal_to_firefighter(SIGQUIT); //wysylanie sygnału do strażaka
    countdown_to_exit();
}

void set_process_group() {
    pid_t pid = getpid();
    setpgid(pid, pid);  // Ustawienie głównego procesu jako lidera grupy procesów (po to żeby strażak mógł wysłać poprawnie sygnały)
}
