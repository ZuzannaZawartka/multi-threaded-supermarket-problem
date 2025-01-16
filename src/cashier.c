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
#include "customer.h"

extern SharedMemory* shared_mem; // Deklaracja pamięci dzielonej
extern pthread_mutex_t mutex;
extern int current_cashiers;
extern pthread_cond_t cashier_closed_cond ;
pthread_key_t cashier_thread_key;

volatile int terminate_flags[MAX_CASHIERS] = {0}; 

void init_cashier_thread_key() {
    if (pthread_key_create(&cashier_thread_key, NULL) != 0) {
        perror("Błąd tworzenia klucza dla wątku kasjera");
        exit(1);
    }
}

void create_cashier(pthread_t* cashier_thread, int* cashier_id) {

    static int initialized = 0;
    if (!initialized) {
        init_cashier_thread_key(); // Inicjalizuj klucz wątku kasjera
        initialized = 1;
    }

    // Tworzenie kolejki komunikatu dla kasjera
    int queue_id = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (queue_id == -1) {
        perror("Błąd tworzenia kolejki komunikatów");
        exit(1);
    }

    // Zapisanie identyfikatora kolejki w pamięci dzielonej
    set_queue_id(shared_mem, *cashier_id , queue_id);


    // Przypisanie ID kasjera do wątku
    if (pthread_setspecific(cashier_thread_key, cashier_id) != 0) {
        perror("Błąd przypisania ID kasjera do wątku");
        exit(1);
    }

    // Tworzenie wątku kasjera
    if (pthread_create(cashier_thread, NULL, cashier_function, cashier_id) != 0) {
        perror("Błąd tworzenia wątku kasjera");
        exit(1);
    }

    printf("Kasjer %d został uruchomiony.\n", *cashier_id);
}


void* cashier_function(void* arg) {
    int cashier_id = *((int*)arg); 

    // Przypisanie ID kasjera do wątku
    if (pthread_setspecific(cashier_thread_key, &cashier_id) != 0) {
        perror("Błąd przypisania ID kasjera do wątku");
        exit(1);
    }

    // Rejestracja handlera sygnału SIGUSR1
    signal(SIGHUP, handle_cashier_signal);
    signal(SIGUSR1, sigUsr2Handler);

    Message message;

    // Pobieranie identyfikatora kolejki kasjera z pamięci dzielonej
    int queue_id = get_queue_id(shared_mem, cashier_id);


    while (1) {
        // Próba odebrania wiadomości z kolejki
        if (msgrcv(queue_id, &message, sizeof(message) - sizeof(long), cashier_id, IPC_NOWAIT) == -1) {
           if (errno == ENOMSG) {
                // Sprawdzenie flagi końca pracy
                 pthread_mutex_lock(&mutex);
                int terminate_flag = terminate_flags[cashier_id - 1];
                pthread_mutex_unlock(&mutex);

                if (terminate_flag == 1) { //jesli zakonczenie pracy a juz nie ma wiadomosci to mozemy usuwac kasjera
                    struct msqid_ds queue_info;

                    // Sprawdzamy, czy kolejka jest pusta
                    if (msgctl(queue_id, IPC_STAT, &queue_info) == -1) {
                        perror("Błąd pobierania informacji o kolejce");
                        exit(1);
                    }

                    if (queue_info.msg_qnum == 0) {
                        // Czyszczenie kolejki
                       
                        cleanup_queue(cashier_id);

                        // Sygnalizujemy zakończenie pracy kasjera
                        pthread_mutex_lock(&mutex);
                        pthread_cond_signal(&cashier_closed_cond);
                        pthread_mutex_unlock(&mutex);

                        // wyzerowanie flagi
                        pthread_mutex_lock(&mutex);
                        terminate_flags[cashier_id-1]=0;
                        pthread_mutex_unlock(&mutex);

                        printf("KASJER %d obsłużył wszystkich\n\n", cashier_id);
                        pthread_exit(NULL);
                    }
                }

                sleep(1);  // Jeśli nie ma wiadomości, kasjer czeka
                continue;
           }
            perror("Błąd odbierania komunikatu kasjer");
            exit(1);
        }

        printf("Kasjer %d obsługuje klienta o PID = %d\n", cashier_id, message.customer_pid);

       
        // Wysłanie odpowiedzi do klienta
        message.mtype = message.customer_pid;
        if (msgsnd(queue_id, &message, sizeof(message) - sizeof(long), 0) == -1) {
            perror("Błąd wysyłania komunikatu do klienta");
            exit(1);
        }

        sleep(4);

        printf("Kasjer %d zakończył obsługę klienta o PID = %d, klient wychodzi \n", cashier_id, message.customer_pid);
    }

    // cleanup_queue(cashier_id);  // Czyszczenie kolejki kasjera
    // decrement_cashiers();
    // pthread_exit(NULL);
}


void sigUsr2Handler(int signum) {
    int cashier_id = *((int*)pthread_getspecific(cashier_thread_key));
    printf("Kasjer %d otrzymał sygnał do zakończenia pracy.\n", cashier_id);
    pthread_mutex_lock(&mutex);
    terminate_flags[cashier_id-1]=1;
    pthread_mutex_unlock(&mutex);
}


// void sigUsr2Handler(int signum) {

//     int cashier_id = *((int*)pthread_getspecific(cashier_thread_key)); //potrzebne ponieważ cashier id sie zmieniaja a skoro jest sygnal do tego konktetnego to ma być ono usuniete


//     // Pobranie identyfikatora kolejki kasjera
//     int queue_id = get_queue_id(shared_mem, cashier_id);

//     printf("ZAMYKANIE KASY NR : %d  - Zakończę obsługę wszystkich klientów w kolejce.\n", cashier_id);

//     Message message;

//     // Obsługuje wszystkich klientów w kolejce
//     while (msgrcv(queue_id, &message, sizeof(message) - sizeof(long),cashier_id, IPC_NOWAIT) != -1) {
//         printf("Kasjer obsługujący klienta o PID = %d.\n", message.customer_pid);
        
//         // Wysłanie komunikatu o zakończeniu obsługi do klienta
//         message.mtype = message.customer_pid;
//         if (msgsnd(queue_id, &message, sizeof(message) - sizeof(long), 0) == -1) {
//             perror("Błąd wysyłania komunikatu do klienta");
//             exit(1);
//         }

//         sleep(3);  // Symulacja obsługi


//         printf("Kasjer %d zakończył obsługę klienta o PID = %d, klient wychodzi \n", cashier_id, message.customer_pid);
//     }

//     // Oczekiwanie aż kolejka będzie naprawdę pusta
//     struct msqid_ds queue_info;
//     do {
//         if (msgctl(queue_id, IPC_STAT, &queue_info) == -1) {
//             perror("Błąd pobierania informacji o kolejce");
//             exit(1);
//         }

//         if (queue_info.msg_qnum > 0) {
//             printf("Kolejka kasjera %d nadal zawiera %lu komunikatów, czekam...\n", cashier_id, queue_info.msg_qnum);
//             sleep(1);  // Odczekaj chwilę przed ponownym sprawdzeniem
//         }
//     } while (queue_info.msg_qnum > 0);

//     printf("Kolejka kasjera %d jest pusta. Można bezpiecznie zamknąć kasę.\n", cashier_id);

//     pthread_mutex_lock(&mutex);
//     pthread_cond_signal(&cashier_closed_cond);
//     pthread_mutex_unlock(&mutex);

//     // Po zakończeniu obsługi wszystkich klientów kasjer kończy pracę
//     printf("ZAKONCZONO OBSLUGE KLIENTOW, Kasjer zakończył obsługę wszystkich klientów.\n");
//     cleanup_queue(cashier_id);
//     pthread_exit(NULL);
// }


void handle_cashier_signal(int sig) {
     int cashier_id = *((int*)pthread_getspecific(cashier_thread_key));
    printf("MAM SYGNAL  kasjer:%d:)\n",cashier_id);

    while (get_customers_in_shop() > 0 ) {
        printf("czekamy na wyjscie wszystkich klientow pozostalo : %d \n",get_customers_in_shop() );
        sleep(1);
    }
    decrement_cashiers(); // Zmniejszamy liczbę kasjerów
    printf("KASJER WYSZEDL %d\n",cashier_id);
    pthread_exit(NULL);  // Kasjer kończy pracę
}

//TU COS NIE GRA
void wait_for_cashiers(pthread_t* cashier_threads, int num_cashiers) {
    void* status;
    for (int i = 0; i <= num_cashiers; i++) {
        pthread_t cashier_thread = get_cashier_thread(cashier_threads, i);

        // Sprawdzamy, czy wątek kasjera jest zainicjowany
        if (cashier_thread != 0) {
            int ret = pthread_join(cashier_thread, &status);  // Czeka na zakończenie wątku
            if (ret == 0) {
                printf("Wątek kasjera %d zakończył się z kodem: %ld, ttid: %lu\n", i, (long)status, cashier_thread);
            } else {
                printf("Błąd podczas oczekiwania na zakończenie wątku kasjera %d\n", i);
            }
        }
    }
}


void cleanup_queue(int cashier_id) {
    // Pobranie identyfikatora kolejki kasjera z pamięci dzielonej
    int queue_id = get_queue_id(shared_mem, cashier_id);

    if (queue_id == -1) {
        return; //kolejka nie istnieje
    }
    if (msgctl(queue_id, IPC_RMID, NULL) == -1) {
        perror("Błąd usuwania kolejki komunikatów");
    } else {
        printf("Kolejka komunikatów dla kasjera %d została usunięta.\n", cashier_id);

        // Zaktualizowanie pamięci dzielonej: ustawienie kolejki na -1
        set_queue_id(shared_mem, cashier_id, -1);
    }
}

//wywolywane z poziomu manadzera
void cleanAfterCashiers() {
    for (int i = 1; i <= MAX_CASHIERS; i++) { //od jedynki ponieważ kasjerzy mają id od 1 do 10
        cleanup_queue(i);
    }
}
