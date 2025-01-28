#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include "cashier.h"
#include "shared_memory.h"
#include "manager_cashiers.h"
#include "creator_customer.h"
#include "customer.h"
#include <stdatomic.h>

extern SharedMemory* shared_mem;                // Wskaźnik do pamięci dzielonej
extern pthread_mutex_t mutex;                   // Mutex do synchronizacji wątków
extern int current_cashiers;                    // Liczba aktualnie działających kasjerów
extern int terminate_flags[MAX_CASHIERS];       // Flagi końca pracy dla kasjerów
extern pthread_t cashier_threads[MAX_CASHIERS]; // Tablica wątków kasjerów

/**
 * @brief Tworzy wątek kasjera oraz przypisaną mu kolejkę komunikatów.
 * 
 * @param cashier_thread Wskaźnik do wątku kasjera.
 * @param cashier_id Wskaźnik do ID kasjera.
 * 
 * @details Tworzy kolejkę komunikatów dla danego kasjera i zapisuje jej ID w pamięci dzielonej.
 * Następnie inicjalizuje wątek kasjera.
 */
void create_cashier(pthread_t* cashier_thread, int* cashier_id) {

    // Tworzenie kolejki komunikatu dla kasjera
    int queue_id = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
    if (queue_id == -1) {
        perror("Błąd tworzenia kolejki komunikatów");
        pthread_exit(NULL);
    }

    // Zapisanie identyfikatora kolejki w pamięci dzielonej
    set_queue_id(shared_mem, *cashier_id , queue_id);

    // Tworzenie wątku kasjera
    if (pthread_create(cashier_thread, NULL, cashier_function, cashier_id) != 0) {
        perror("Błąd tworzenia wątku kasjera");
        pthread_exit(NULL);
    }
    printf("Kasjer %d został uruchomiony.\n", *cashier_id);
}

/**
 * @brief Funkcja obsługująca działanie wątku kasjera.
 * 
 * @param arg Wskaźnik do ID kasjera.
 * @return Zwraca NULL po zakończeniu działania wątku.
 * 
 * @details Kasjer obsługuje klientów na podstawie wiadomości w jego kolejce komunikatów.
 * Po zakończeniu obsługi, wątek kasjera kończy się.
 */
void* cashier_function(void* arg) {

    srand(time(NULL));

    int cashier_id = *((int*)arg);  //pobranie cashier_id z argumentów

    // Rejestracja handlera sygnału SIGHUP
    struct sigaction sa;
    sa.sa_handler = handle_cashier_signal_fire;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("Błąd rejestracji handlera sygnału SIGHUP");
        pthread_exit(NULL);
    }

    // Pobieranie identyfikatora kolejki kasjera z pamięci dzielonej
    int queue_id = get_queue_id(shared_mem, cashier_id);

    Message message;
    while (get_fire_flag(shared_mem)!=1) {
        if (msgrcv(queue_id, &message, sizeof(message) - sizeof(long), cashier_id, IPC_NOWAIT) == -1) {  // Próba odebrania wiadomości z kolejki
           if (errno == ENOMSG) {
                
                int ret = pthread_mutex_lock(&mutex);
                if (ret != 0) {
                    perror("Błąd podczas blokowania mutexa");
                    pthread_exit(NULL);
                }
                
                int terminate_flag = terminate_flags[cashier_id - 1];       // Sprawdzenie flagi końca pracy
                
                ret = pthread_mutex_unlock(&mutex);
                if (ret != 0) {
                    perror("Błąd podczas zwalniania mutexa");
                    pthread_exit(NULL);
                }

                if (terminate_flag == 1) {                                  // Jeśli flaga zakończenia pracy jest aktywna a już nie ma wiadomości to możemy usuwać kasjera.
                    struct msqid_ds queue_info;
                    if (msgctl(queue_id, IPC_STAT, &queue_info) == -1) {    // Sprawdzamy, czy kolejka jest pusta
                        perror("Błąd pobierania informacji o kolejce");
                        pthread_exit(NULL);
                    }

                    if (queue_info.msg_qnum == 0) {                         // Kolejka jest pusta
                    
                        cleanup_queue(cashier_id);                          // Czyszczenie kolejki komunikatów

                        int ret = pthread_mutex_lock(&mutex);
                        if (ret != 0) {
                            perror("Błąd podczas blokowania mutexa");
                            pthread_exit(NULL);
                        }
                        terminate_flags[cashier_id-1]=0;                    // Wyzerowanie flagi (dla kolejnych kasjerów którzy będą używać tego id)
                        ret = pthread_mutex_unlock(&mutex);
                        if (ret != 0) {
                            perror("Błąd podczas zwalniania mutexa");
                            pthread_exit(NULL);
                        }

                        printf("\033[31m[KASJER %d] obsłużył wszystkich\033[0m\n\n", cashier_id);

                        pthread_exit(NULL);//koniec wątku kasjera
                    }
                }
                usleep(10000); // Jeśli nie ma wiadomości, kasjer czeka
                continue;
            }
            else if (errno == EINTR) {
                continue;
            }
            perror("Błąd odbierania komunikatu kasjer");
            pthread_exit(NULL);
        }

        printf("Kasjer %d obsługuje klienta o PID = %d\n", cashier_id, message.customer_pid);

        // Uśpienie na losowy czas
        int stay_time_in_microseconds = generate_random_time(MIN_CASHIER_OPERATION, MAX_CASHIER_OPERATION);
        usleep(stay_time_in_microseconds);

        // Wysłanie odpowiedzi do klienta
        message.mtype = message.customer_pid;
        if (msgsnd(queue_id, &message, sizeof(message) - sizeof(long), 0) == -1) {
            perror("Błąd wysyłania komunikatu do klienta");
            pthread_exit(NULL);
        }
        printf("Kasjer %d zakończył obsługę klienta o PID = %d \n", cashier_id, message.customer_pid);
    }

    pthread_exit(NULL);
}

/**
 * @brief Obsługuje sygnał SIGHUP dla kasjera w sytuacji pożaru.
 * 
 * @param sig Numer odebranego sygnału.
 * 
 * @details Po odebraniu sygnału kasjer natychmiast kończy pracę.
 */
void handle_cashier_signal_fire(int sig) {
    pthread_exit(NULL);
}

/**
 * @brief Oczekuje na zakończenie działania wszystkich wątków kasjerów.
 * 
 * @param cashier_threads Tablica wątków kasjerów.
 * @param num_cashiers Liczba aktualnych kasjerów.
 */
void wait_for_cashiers(pthread_t* cashier_threads,int num_cashiers) {
    void* status;
    for (int i = 0; i < num_cashiers; i++) { //zmienione na < z <=
        pthread_t cashier_thread = get_cashier_thread(cashier_threads, i);

        // Sprawdzamy, czy wątek kasjera jest zainicjowany
        if (cashier_thread != 0) {
            int ret = pthread_join(cashier_thread, &status);  // Czeka na zakończenie wątku
            if (ret == 0) {
                printf("Wątek kasjera %d zakończył się z kodem: %ld, ttid: %lu\n", i+1, (long)status, cashier_thread);
                decrement_cashiers();
            } else {
                // Obsługuje błąd w przypadku niepowodzenia w oczekiwaniu na wątek
                fprintf(stderr, "Błąd podczas oczekiwania na wątek kasjera %d: %d\n", i+1, ret);
            }
        }else{
            printf("Wątek kasjera %d zakończył się wcześniej\n",i+1);
            decrement_cashiers();
        }
    }
}

/**
 * @brief Usuwa kolejkę komunikatów przypisaną do kasjera.
 * 
 * @param cashier_id ID kasjera.
 */
void cleanup_queue(int cashier_id) {
    int queue_id = get_queue_id(shared_mem, cashier_id);

    if (queue_id == -1) {
        return; //kolejka nie istnieje
    }
    if (msgctl(queue_id, IPC_RMID, NULL) == -1) {
        perror("Błąd usuwania kolejki komunikatów");
    } else {
        set_queue_id(shared_mem, cashier_id, -1);  // Zaktualizowanie pamięci dzielonej: ustawienie kolejki na -1
    }
}

/**
 * @brief Usuwa wszystkie kolejki komunikatów przypisane do kasjerów.
 */
void cleanAfterCashiers() {
    for (int i = 1; i <= MAX_CASHIERS; i++) { // Od jedynki ponieważ kasjerzy mają id od 1 do 10
        cleanup_queue(i);
    }
}
