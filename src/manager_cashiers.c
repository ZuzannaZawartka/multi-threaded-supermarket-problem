#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "manager_cashiers.h"
#include "cashier.h"
#include <signal.h>

#include "shared_memory.h"

#define MIN_PEOPLE_FOR_CASHIER 5  // Na każdą grupę 5 klientów przypada jeden kasjer
#define MAX_CASHIERS 10 
#define MIN_CASHIERS 2

extern SharedMemory* shared_mem;  // Deklaracja pamięci dzielonej


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_t cashier_threads[MAX_CASHIERS];
int cashier_ids[MAX_CASHIERS];

int current_cashiers = 0;  // Liczba aktywnych kasjerów



void send_signal_to_cashiers(int signal) {
    int sum_cash = get_current_cashiers();
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < sum_cash; i++) {
        if (pthread_kill(cashier_threads[i], signal) != 0) {
            perror("Błąd wysyłania sygnału do kasjera");
        }
    }
    pthread_mutex_unlock(&mutex);
    
}

// Przykład: Wywołanie send_signal_to_cashiers() w odpowiedzi na SIGINT
void sigint_handler2(int signum) {
    send_signal_to_cashiers(SIGHUP);  
}


void* manage_customers(void* arg) {
    
    signal(SIGTERM, sigint_handler2); 

    create_initial_cashiers(cashier_threads,cashier_ids,&current_cashiers);

    wait_for_cashiers(cashier_threads, current_cashiers); 

    printf("Menadżer zakończył działanie.\n");

    return NULL;
}

void create_initial_cashiers(pthread_t* cashier_threads, int* cashier_ids, int* current_cashiers) {
    for (int i = 0; i < MIN_CASHIERS; i++) {
        cashier_ids[i] = i + 1;  // Identyfikator kasjera (liczony od 1)
        create_cashier(&cashier_threads[i], &cashier_ids[i]);
        increment_cashiers();  // Zwiększamy liczbę kasjerów
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
    printf("Wątek menedżera kasjerów utworzony, id wątku: %ld\n", *manager_thread);
}



void terminate_manager(pthread_t manager_thread) { 
    pthread_join(manager_thread, NULL);  // Czekanie na zakończenie wątku menedżera
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);  // Usuwamy zmienną warunkową
}



void increment_cashiers() {
    int ret = pthread_mutex_lock(&mutex);
    if (ret != 0) {
        perror("Błąd podczas blokowania mutexa w increment_cashiers");
        exit(1);  // Można wybrać inne działanie w przypadku błędu
    }

    current_cashiers++;

    ret = pthread_mutex_unlock(&mutex);
    if (ret != 0) {
        perror("Błąd podczas zwalniania mutexa w increment_cashiers");
        exit(1);  // Można wybrać inne działanie w przypadku błędu
    }
}

void decrement_cashiers() {
    int ret = pthread_mutex_lock(&mutex);
    if (ret != 0) {
        perror("Błąd podczas blokowania mutexa w decrement_cashiers");
        exit(1);  // Można wybrać inne działanie w przypadku błędu
    }

    current_cashiers--;

    ret = pthread_mutex_unlock(&mutex);
    if (ret != 0) {
        perror("Błąd podczas zwalniania mutexa w decrement_cashiers");
        exit(1);  // Można wybrać inne działanie w przypadku błędu
    }
}

int get_current_cashiers() {
    int count;
    int ret = pthread_mutex_lock(&mutex);
    if (ret != 0) {
        perror("Błąd podczas blokowania mutexa w get_current_cashiers");
        exit(1);  // Można wybrać inne działanie w przypadku błędu
    }

    count = current_cashiers;

    ret = pthread_mutex_unlock(&mutex);
    if (ret != 0) {
        perror("Błąd podczas zwalniania mutexa w get_current_cashiers");
        exit(1);  // Można wybrać inne działanie w przypadku błędu
    }

    return count;
}



pthread_t get_cashier_thread(pthread_t* cashier_threads, int index) {
    pthread_t thread;
    int ret = pthread_mutex_lock(&mutex);  // Zabezpieczenie przed równoczesnym dostępem do tablicy
    if (ret != 0) {
        perror("Błąd podczas blokowania mutexa w get_cashier_thread");
        exit(1);  // Można wybrać inne działanie w przypadku błędu
    }

    thread = cashier_threads[index];

    ret = pthread_mutex_unlock(&mutex);  // Zwolnienie mutexu
    if (ret != 0) {
        perror("Błąd podczas zwalniania mutexa w get_cashier_thread");
        exit(1);  // Można wybrać inne działanie w przypadku błędu
    }

    return thread;
}