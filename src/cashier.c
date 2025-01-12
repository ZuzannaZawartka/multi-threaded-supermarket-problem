#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <pthread.h>
#include "cashier.h"
#include <errno.h>
#include "shared_memory.h"

// Tablica przechowująca identyfikatory kolejek dla kasjerów (max 10)
extern SharedMemory* shared_mem; // Deklaracja pamięci dzielonej

void create_cashier(pthread_t* cashier_thread, int* cashier_id) {
    // Tworzenie kolejki komunikatu dla kasjera
    int queue_id = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (queue_id == -1) {
        perror("Błąd tworzenia kolejki komunikatów");
        exit(1);
    }

    // Zapisanie identyfikatora kolejki w pamięci dzielonej
    set_queue_id(shared_mem, *cashier_id - 1, queue_id);

    // Tworzenie wątku kasjera
    if (pthread_create(cashier_thread, NULL, cashier_function, cashier_id) != 0) {
        perror("Błąd tworzenia wątku kasjera");
        exit(1);
    }

    printf("Kasjer %d został uruchomiony.\n", *cashier_id);
}

void* cashier_function(void* arg) {
    int cashier_id = *((int*)arg);  // Kasjer ma swoje ID
    Message message;

    // Pobieranie identyfikatora kolejki kasjera z pamięci dzielonej
    int queue_id = get_queue_id(shared_mem, cashier_id - 1);

    while (1) {
        
        //sprawdzamy czy kasjer ma skonczyc prace
        if (get_should_exit(shared_mem)) {
            // Jeśli liczba klientów wynosi 0, kasjer czeka
            if (get_number_customer(shared_mem) == 0) {
                break;  // Kasjer czeka, aż liczba klientów będzie 0
            }
            sleep(1);
            continue;  // Zakończenie pracy kasjera, jesli klienci juz wyszli
        }

        // Próba odebrania wiadomości z kolejki
        if (msgrcv(queue_id, &message, sizeof(message) - sizeof(long), cashier_id, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                sleep(1);  // Jeśli nie ma wiadomości, kasjer czeka
                continue;
            }
            perror("Błąd odbierania komunikatu");
            exit(1);
        }

        printf("Kasjer %d obsługuje klienta o PID = %d\n", cashier_id, message.customer_pid);

        sleep(1);


         //sprawdzamy czy kasjer ma skonczyc prace
        if (get_should_exit(shared_mem)) {
            // Jeśli liczba klientów wynosi 0, kasjer czeka
            if (get_number_customer(shared_mem) == 0) {
                break;  // Kasjer czeka, aż liczba klientów będzie 0
            }
            sleep(1);
            continue;  // Zakończenie pracy kasjera, jesli klienci juz wyszli
        }
        
        // Wysłanie odpowiedzi do klienta
        message.mtype = message.customer_pid;
        if (msgsnd(queue_id, &message, sizeof(message) - sizeof(long), 0) == -1) {
            perror("Błąd wysyłania komunikatu do klienta");
            exit(1);
        }

        printf("Kasjer %d zakończył obsługę klienta o PID = %d\n", cashier_id, message.customer_pid);
    }

    cleanup_queue(cashier_id);  // Czyszczenie kolejki kasjera

    pthread_exit(NULL);
}

// // Funkcja do tworzenia wszystkich kasjerów
// void init_cashiers(pthread_t* cashier_threads, int* cashier_ids, int num_cashiers) {
//     // Inicjalizacja kasjerów
//     for (int i = 0; i < num_cashiers; i++) {
//         cashier_ids[i] = i + 1; 
//         init_cashier(cashier_ids[i]);  // Inicjalizacja kasjera z kolejką

//         // Tworzenie wątku kasjera
//         if (pthread_create(&cashier_threads[i], NULL, cashier_function, &cashier_ids[i]) != 0) {
//             perror("Błąd tworzenia wątku kasjera");
//             exit(1);
//         }
//     }
// }

// Czyszczenie kolejki kasjera
void cleanup_queue(int cashier_id) {
    // Pobranie identyfikatora kolejki kasjera z pamięci dzielonej
    int queue_id = get_queue_id(shared_mem, cashier_id - 1);

    if (msgctl(queue_id, IPC_RMID, NULL) == -1) {
        perror("Błąd usuwania kolejki komunikatów");
    } else {
        // printf("Kolejka komunikatów dla kasjera %d została usunięta.\n", cashier_id-1);
    }
}

// Funkcja do oczekiwania na zakończenie pracy kasjerów
void wait_for_cashiers(pthread_t* cashier_threads, int num_cashiers) {
    void* status;
    for (int i = 0; i < num_cashiers; i++) {
        int ret = pthread_join(cashier_threads[i], &status);  // Czeka na zakończenie wątku
        if (ret == 0) {
            printf("Wątek kasjera %d zakończył się z kodem: %ld , ttid:%lu\n", i, (long)status, cashier_threads[i]);
        } else {
            printf("Błąd podczas oczekiwania na zakończenie wątku kasjera %d\n", i);
        }
    }
}
