#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <pthread.h>
#include "cashier.h"
#include <errno.h>
#include "shared_memory.h"
#include <signal.h>
#include "manager_cashiers.h"


// Tablica przechowująca identyfikatory kolejek dla kasjerów (max 10)
extern SharedMemory* shared_mem; // Deklaracja pamięci dzielonej


extern pthread_mutex_t mutex;
extern pthread_cond_t cond;
extern int current_cashiers;


void create_cashier(pthread_t* cashier_thread, int* cashier_id) {
    // Tworzenie kolejki komunikatu dla kasjera
    int queue_id = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (queue_id == -1) {
        perror("Błąd tworzenia kolejki komunikatów");
        exit(1);
    }

    // Zapisanie identyfikatora kolejki w pamięci dzielonej
    set_queue_id(shared_mem, *cashier_id , queue_id);

    // Tworzenie wątku kasjera
    if (pthread_create(cashier_thread, NULL, cashier_function, cashier_id) != 0) {
        perror("Błąd tworzenia wątku kasjera");
        exit(1);
    }

    printf("Kasjer %d został uruchomiony.\n", *cashier_id);
}


void* cashier_function(void* arg) {
    int cashier_id = *((int*)arg); 

    // Rejestracja handlera sygnału SIGUSR1
    signal(SIGHUP, handle_cashier_signal);

    Message message;

    // Pobieranie identyfikatora kolejki kasjera z pamięci dzielonej
    int queue_id = get_queue_id(shared_mem, cashier_id);


    while (1) {
        // Próba odebrania wiadomości z kolejki
        if (msgrcv(queue_id, &message, sizeof(message) - sizeof(long), cashier_id, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                sleep(1);  // Jeśli nie ma wiadomości, kasjer czeka
                continue;
            }
            perror("Błąd odbierania komunikatu kasjer");
            exit(1);
        }

        printf("Kasjer %d obsługuje klienta o PID = %d\n", cashier_id, message.customer_pid);

        sleep(1);

        
        // Wysłanie odpowiedzi do klienta
        message.mtype = message.customer_pid;
        if (msgsnd(queue_id, &message, sizeof(message) - sizeof(long), 0) == -1) {
            perror("Błąd wysyłania komunikatu do klienta");
            exit(1);
        }

        printf("Kasjer %d zakończył obsługę klienta o PID = %d\n", cashier_id, message.customer_pid);
    }

    cleanup_queue(cashier_id);  // Czyszczenie kolejki kasjera
    decrement_cashiers();

    pthread_exit(NULL);
}

void handle_cashier_signal(int sig) {
    //DOPISAC CZYSZCZENIE KOLEJEK
    pthread_exit(NULL);  // Kasjer kończy pracę
}


// Czyszczenie kolejki kasjera
void cleanup_queue(int cashier_id) {
    // Pobranie identyfikatora kolejki kasjera z pamięci dzielonej
    int queue_id = get_queue_id(shared_mem, cashier_id);

    if (msgctl(queue_id, IPC_RMID, NULL) == -1) {
        perror("Błąd usuwania kolejki komunikatów");
    } else {
         printf("Kolejka komunikatów dla kasjera %d została usunięta.\n", cashier_id);
    }
}

void wait_for_cashiers(pthread_t* cashier_threads, int num_cashiers) {
    void* status;
    for (int i = 0; i < num_cashiers; i++) {
        // Używamy funkcji get_cashier_thread do bezpiecznego odczytu wątku kasjera
        pthread_t cashier_thread = get_cashier_thread(cashier_threads, i);

        int ret = pthread_join(cashier_thread, &status);  // Czeka na zakończenie wątku
        if (ret == 0) {
            printf("Wątek kasjera %d zakończył się z kodem: %ld , ttid:%lu\n", i, (long)status, cashier_thread);
        } else {
            printf("Błąd podczas oczekiwania na zakończenie wątku kasjera %d\n", i);
        }
    }
}
