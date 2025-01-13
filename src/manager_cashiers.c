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

    for (int i = 0; i < sum_cash; i++) {
        pthread_t cashier_thread = get_cashier_thread(cashier_threads, i);  // funkcja pomocznicza uzywajaca mutexa
        
        //wysylamy sygnal do kasjera
        if (pthread_kill(cashier_thread, signal) != 0) {
            perror("Error sending signal to cashier thread");
        }
    }
}

// Przykład: Wywołanie send_signal_to_cashiers() w odpowiedzi na SIGINT
void sigTermHandler(int signum) {
    send_signal_to_cashiers(SIGHUP);  
}


void* manage_customers(void* arg) {
    
    signal(SIGTERM, sigTermHandler); 

    create_initial_cashiers(cashier_threads,cashier_ids);

    wait_for_cashiers(cashier_threads, current_cashiers); 

    printf("Menadżer zakończył działanie.\n");

    return NULL;
}

// void create_initial_cashiers(pthread_t* cashier_threads, int* cashier_ids) {
//     for (int i = 0; i < MIN_CASHIERS; i++) {
//         cashier_ids[i] = i + 1;  // Identyfikator kasjera (liczony od 1)
//         create_cashier(&cashier_threads[i], &cashier_ids[i]);
//         increment_cashiers();  // Zwiększamy liczbę kasjerów
//     }
//     printf("Utworzono minimalną liczbę kasjerów: %d\n", MIN_CASHIERS);

// }

void create_initial_cashiers(pthread_t* cashier_threads, int* cashier_ids) {
    for (int i = 0; i < MIN_CASHIERS; i++) {

        set_cashier_id(cashier_ids, i, i + 1);  // Przypisanie ID kasjera

        
        int* cashier_id = get_cashier_id_pointer(cashier_ids, i);  // Pobranie ID kasjera


        pthread_t cashier_thread;
        create_cashier(&cashier_thread, cashier_id);  // Tworzymy wątek kasjera

        // Ustawiamy wątek kasjera w tablicy (z mutexem)
        set_cashier_thread(cashier_threads, i, cashier_thread);

        increment_cashiers();  // Zwiększamy liczbę kasjerów
    }
    printf("Utworzono minimalną liczbę kasjerów: %d\n", MIN_CASHIERS);
}

//TODO OGARNAC MUTEXY NA WATKACH ZEBY WSZYSTKO BYLO POD MUTEXAMI

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

// Funkcja przypisująca wątek kasjera do tablicy cashier_threads
void set_cashier_thread(pthread_t* cashier_threads, int index, pthread_t thread) {
    pthread_mutex_lock(&mutex);  // 
    cashier_threads[index] = thread;  // Ustawienie wątku kasjera w tablicy
    pthread_mutex_unlock(&mutex);  
}


int get_cashier_id(int* cashier_ids, int index) {
    int cashier_id;
    
    // Blokujemy mutex, aby zabezpieczyć dostęp do tablicy cashier_ids
    int ret = pthread_mutex_lock(&mutex);  
    if (ret != 0) {
        perror("Error while locking mutex in get_cashier_id");
        exit(1);  // Możesz też zrobić coś innego, np. zwrócić -1 w przypadku błędu
    }

    cashier_id = cashier_ids[index];  // Pobieramy ID kasjera z tablicy

    ret = pthread_mutex_unlock(&mutex);  // Zwolnienie mutexu
    if (ret != 0) {
        perror("Error while unlocking mutex in get_cashier_id");
        exit(1);  // Możesz też zrobić coś innego w przypadku błędu
    }

    return cashier_id;  // Zwracamy ID kasjera
}


void set_cashier_id(int* cashier_ids, int index, int id) {
    pthread_mutex_lock(&mutex);  // Zabezpieczenie przed równoczesnym dostępem
    cashier_ids[index] = id;  // Ustawienie ID kasjera w tablicy
    pthread_mutex_unlock(&mutex);  // Zwolnienie mutexa
}


// Funkcja zwracająca wskaźnik do elementu tablicy cashier_ids
int* get_cashier_id_pointer(int* cashier_ids, int index) {
    int* cashier_id = NULL;
    
  
    pthread_mutex_lock(&mutex);  
    
    // Zwracamy wskaźnik do odpowiedniego elementu
    cashier_id = &cashier_ids[index];
    
    pthread_mutex_unlock(&mutex);  // Zwolnienie mutexu

    return cashier_id;
}
