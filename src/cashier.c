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

// Tablica przechowująca identyfikatory kolejek dla kasjerów (max 10)
int queue_ids[MAX_CASHIERS];

extern SharedMemory* shared_mem; // Deklaracja pamięci dzielonej

void init_cashier(int cashier_id) {
    // Tworzenie kolejki komunikatu kasjera
    queue_ids[cashier_id - 1] = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (queue_ids[cashier_id - 1] == -1) {
        perror("Błąd tworzenia kolejki komunikatów");
        exit(1);
    }
}

void* cashier_function(void* arg) {
    int cashier_id = *((int*)arg);  // Kasjer ma swoje ID
    Message message;

    while (1) {
        // Sprawdzanie flagi should_exit z pamięci dzielonej
        if (get_should_exit(shared_mem)) {
            break;  // Zakończenie pracy kasjera
        }

        if (msgrcv(queue_ids[cashier_id - 1], &message, sizeof(message) - sizeof(long), cashier_id, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                sleep(1);//jesli bylaby wiadomosc o zakonczeniu pracy to utknalby oczekujac na wiadomosc dlatego msgrcv z flaga IPC_NOWAIT
                continue;
            }
            perror("Błąd odbierania komunikatu");
            exit(1);
        }

        printf("Kasjer %d obsługuje klienta o PID = %d\n", cashier_id, message.customer_pid);

        sleep(1);

        message.mtype = message.customer_pid;
        if (msgsnd(queue_ids[cashier_id - 1], &message, sizeof(message) - sizeof(long), 0) == -1) {
            perror("Błąd wysyłania komunikatu do klienta");
            exit(1);
        }

        printf("Kasjer %d zakończył obsługę klienta o PID = %d\n", cashier_id, message.customer_pid);
    }

    cleanup_queue(cashier_id);

    pthread_exit(NULL);
}

// Funkcja do tworzenia wszystkich kasjerów
void init_cashiers(pthread_t* cashier_threads, int* cashier_ids, int num_cashiers) {
    // Inicjalizacja kasjerów
    for (int i = 0; i < num_cashiers; i++) {
        cashier_ids[i] = i + 1; 
        init_cashier(cashier_ids[i]);  // Inicjalizacja kasjera z kolejką

        // Tworzenie wątku kasjera
        if (pthread_create(&cashier_threads[i], NULL, cashier_function, &cashier_ids[i]) != 0) {
            perror("Błąd tworzenia wątku kasjera");
            exit(1);
        }
    }
}

//czyszczenie kolejki komunikatów kasjera
void cleanup_queue(int cashier_id) {
    if (msgctl(queue_ids[cashier_id - 1], IPC_RMID, NULL) == -1) {
        perror("Błąd usuwania kolejki komunikatów");
    } else {
        printf("Kolejka komunikatów dla kasjera %d została usunięta.\n", cashier_id);
    }
}


// Funkcja do oczekiwania na zakończenie pracy kasjerów
void wait_for_cashiers(pthread_t* cashier_threads, int num_cashiers) {
    void* status;
    for (int i = 0; i < num_cashiers; i++) {
        int ret = pthread_join(cashier_threads[i], &status);  // Czeka na zakończenie wątku
        if (ret == 0) {
            printf("Wątek kasjera %d zakończył się z kodem: %ld , ttid:%lu\n", i, (long)status,cashier_threads[i]);
        } else {
            printf("Błąd podczas oczekiwania na zakończenie wątku kasjera %d\n", i);
        }
    }
}
