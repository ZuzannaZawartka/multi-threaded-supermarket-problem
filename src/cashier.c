#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include "cashier.h"
#include <errno.h>  
#include "shared_memory.h"

#include "customer.h"

extern int NUM_CUSTOMERS;

// Tablica przechowująca identyfikatory kolejek dla kasjerów (max 10)
int queue_ids[MAX_CASHIERS];

extern SharedMemory* shared_mem; // Deklaracja pamięci dzielonej

void init_cashier(int cashier_id) {
    // Tworzenie kolejki komunikatu kasjera
    int queue_id = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (queue_id == -1) {
        perror("Błąd tworzenia kolejki komunikatów");
        exit(1);
    }

    set_queue_id(shared_mem, cashier_id - 1, queue_id);
}

void* cashier_function(void* arg) {
    int cashier_id = *((int*)arg);  // Kasjer ma swoje ID

    int queue_id = get_queue_id(shared_mem, cashier_id - 1);//pobieranie kolejki kasjera

    Message message;

    while (1) {
        // Sprawdzanie flagi should_exit z użyciem semafora
        if (get_should_exit(shared_mem)) {
            // printf("Kasjer %d kończy pracę z powodu zakończenia systemu.\n", cashier_id);
            wait_for_customers(NUM_CUSTOMERS);
            break;
        }


      if (msgrcv(queue_id, &message, sizeof(message) - sizeof(long), 0, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                sleep(1);
                continue;
            }
            perror("Błąd odbierania komunikatu");
            exit(1);
        }

        printf("Kasjer %d obsługuje klienta o PID = %d\n", cashier_id, message.customer_pid);

        sleep(1);

        message.mtype = message.customer_pid;
        if (msgsnd(queue_id, &message, sizeof(message) - sizeof(long), 0) == -1) {
            perror("Błąd wysyłania komunikatu do klienta");
            exit(1);
        }


        //TYMCZASOWO POMYSLE JAK TO INACZEJ ROZWIAZAC
        if (get_should_exit(shared_mem)) {
            // printf("Kasjer %d kończy pracę z powodu zakończenia systemu.\n", cashier_id);

            //TO TRZEBA BEDZIE CZYMS W PAMIECI DZIELONEJ SPRAWDZAC CZY WSZYSCY WYSZLI BO BEZSENSU TO TU WYWOLYWAC
            wait_for_customers(NUM_CUSTOMERS);
            break;
        }

        printf("Kasjer %d zakończył obsługę klienta o PID = %d\n", cashier_id, message.customer_pid);
    }

    cleanup_queue(cashier_id);

    pthread_exit(NULL);
}

// Funkcja do tworzenia wszystkich kasjerów
void init_cashiers(pthread_t* cashier_threads, int* cashier_ids, int num_cashiers) {
     for (int i = 0; i < num_cashiers; i++) {
        cashier_ids[i] = i + 1;
        init_cashier(cashier_ids[i]);

        if (pthread_create(&cashier_threads[i], NULL, cashier_function, &cashier_ids[i]) != 0) {
            perror("Błąd tworzenia wątku kasjera");
            exit(1);
        }
    }
}

//czyszczenie kolejki komunikatów kasjera
void cleanup_queue(int cashier_id) {
    int queue_id = get_queue_id(shared_mem, cashier_id - 1);
    if (msgctl(queue_id, IPC_RMID, NULL) == -1) {
        perror("Błąd usuwania kolejki komunikatów");
    } else {
        printf("Kolejka komunikatów dla kasjera %d została usunięta.\n", cashier_id );
    }
}


// Funkcja do oczekiwania na zakończenie pracy kasjerów
void wait_for_cashiers(pthread_t* cashier_threads, int num_cashiers) {
     void* status;
    for (int i = 0; i < num_cashiers; i++) {
        if (pthread_join(cashier_threads[i], &status) == 0) {
            printf("Wątek kasjera %d zakończył się.\n", i + 1);
        } else {
            perror("Błąd podczas oczekiwania na zakończenie wątku kasjera");
        }
    }
}
