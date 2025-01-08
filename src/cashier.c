#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include "cashier.h"

// Tablica przechowująca identyfikatory kolejek dla kasjerów (max 10)
int queue_ids[MAX_CASHIERS];

void init_cashier(int cashier_id) {
    
    //Tworzenie kolejki komunikatu kasjera
    queue_ids[cashier_id - 1] = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (queue_ids[cashier_id - 1] == -1) {
        perror("Błąd tworzenia kolejki komunikatów");
        exit(1);
    }
}

void* cashier_function(void* arg) {
    int cashier_id = *((int*)arg);  // Kasjer ma swoje ID
    Message message;

    // Kasjer odbiera komunikaty tylko ze swojej kolejki
    while (1) {
        if (msgrcv(queue_ids[cashier_id - 1], &message, sizeof(message) - sizeof(long), cashier_id, 0) == -1) {
            perror("Błąd odbierania komunikatu");
            exit(1);
        }

        printf("Kasjer %d obsługuje klienta o PID = %d\n", cashier_id, message.customer_pid);

        sleep(1);  // Kasjer obsługuje klienta przez sekundę
        printf("Kasjer %d zakończył obsługę klienta o PID = %d\n", cashier_id, message.customer_pid);

        kill(message.customer_pid, SIGTERM);  // Klient dostaje sygnał zakończenia procesu
    }

    return NULL;
}

// Funkcja do tworzenia wszystkich kasjerów
void init_cashiers(pthread_t* cashier_threads, int* cashier_ids, int num_cashiers) {
    // Inicjalizacja kasjerów
    for (int i = 0; i < num_cashiers; i++) {
        cashier_ids[i] = i + 1;  // Numer kasjera (1, 2, 3, ...)
        init_cashier(cashier_ids[i]);  // Inicjalizacja kasjera z kolejką

        // Tworzenie wątku kasjera
        if (pthread_create(&cashier_threads[i], NULL, cashier_function, &cashier_ids[i]) != 0) {
            perror("Błąd tworzenia wątku kasjera");
            exit(1);
        }
    }
}
