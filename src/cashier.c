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
// #include "main.h" //Dla zmiennych długości obsługi przez kasjera

extern SharedMemory* shared_mem; // Deklaracja pamięci dzielonej
extern pthread_mutex_t mutex;
extern int current_cashiers;

pthread_key_t cashier_thread_key;// Klucz dla danych kasjera w pamięci specyficznej dla wątku
extern int terminate_flags[MAX_CASHIERS];


//inicjalizacja klucza który trzyma cashier_id dla wątku
void init_cashier_thread_key() {
    if (pthread_key_create(&cashier_thread_key, NULL) != 0) {
        perror("Błąd tworzenia klucza dla wątku kasjera");
        exit(1);
    }
}

void create_cashier(pthread_t* cashier_thread, int* cashier_id) {



    // Tworzenie kolejki komunikatu dla kasjera
    int queue_id = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
    if (queue_id == -1) {
        perror("Błąd tworzenia kolejki komunikatów");
        exit(1);
    }

    // Zapisanie identyfikatora kolejki w pamięci dzielonej
    set_queue_id(shared_mem, *cashier_id , queue_id);


    // // Przypisanie ID kasjera do wątku
    // if (pthread_setspecific(cashier_thread_key, cashier_id) != 0) {
    //     perror("Błąd przypisania ID kasjera do wątku");
    //     exit(1);
    // }

    // Tworzenie wątku kasjera
    if (pthread_create(cashier_thread, NULL, cashier_function, cashier_id) != 0) {
        perror("Błąd tworzenia wątku kasjera");
        exit(1);
    }

    printf("Kasjer %d został uruchomiony.\n", *cashier_id);
}




void* cashier_function(void* arg) {

    static int initialized = 0;
    if (!initialized) {
        init_cashier_thread_key(); // Inicjalizuj klucz wątku kasjera
        initialized = 1;
    }

    srand(time(NULL));

    int cashier_id = *((int*)arg);  //pobranie cashier_id z argumentów

    // Przypisanie ID kasjera do wątku
    if (pthread_setspecific(cashier_thread_key, &cashier_id) != 0) {
        perror("Błąd przypisania ID kasjera do wątku");
        exit(1);
    }

    // Rejestracja handlera sygnału SIGUSR1 oraz SIGHUP
    if (signal(SIGHUP, handle_cashier_signal_fire)  == SIG_ERR) { //sygnał wyjścia na pożar
        perror("Błąd rejestracji handlera sygnału SIGHUP");
        exit(1);
    }
    // if( signal(SIGUSR1, closeCashier) == SIG_ERR) { //sygnał wyjścia na zamykanie danego kasjera
    //     perror("Błąd rejestracji handlera sygnału SIGUSR1");
    //     exit(1);
    // }; 

    // Pobieranie identyfikatora kolejki kasjera z pamięci dzielonej
    int queue_id = get_queue_id(shared_mem, cashier_id);

    Message message;
    while (1) {
        if (msgrcv(queue_id, &message, sizeof(message) - sizeof(long), cashier_id, IPC_NOWAIT) == -1) {  // Próba odebrania wiadomości z kolejki
           if (errno == ENOMSG) {
                
                int ret = pthread_mutex_lock(&mutex);
                if (ret != 0) {
                    perror("Błąd podczas blokowania mutexa");
                    exit(1);
                }
                int terminate_flag = terminate_flags[cashier_id - 1];// Sprawdzenie flagi końca pracy
                ret = pthread_mutex_unlock(&mutex);
                if (ret != 0) {
                    perror("Błąd podczas zwalniania mutexa");
                    exit(1);
                }

                if (terminate_flag == 1) { //jesli flaga zakończenia pracy jest aktywna a juz nie ma wiadomosci to mozemy usuwac kasjera
                    struct msqid_ds queue_info;


                    // Sprawdzamy, czy kolejka jest pusta
                    if (msgctl(queue_id, IPC_STAT, &queue_info) == -1) {
                        perror("Błąd pobierania informacji o kolejce");
                        exit(1);
                    }

                    if (queue_info.msg_qnum == 0) { //jeśli nie ma wiadomości to czyścimy kasjera
                    
                        cleanup_queue(cashier_id);// Czyszczenie kolejki komunikatów

                        int ret = pthread_mutex_lock(&mutex);
                        if (ret != 0) {
                            perror("Błąd podczas blokowania mutexa");
                            exit(1);
                        }
                        terminate_flags[cashier_id-1]=0; // wyzerowanie flagi
                        ret = pthread_mutex_unlock(&mutex);
                        if (ret != 0) {
                            perror("Błąd podczas zwalniania mutexa");
                            exit(1);
                        }

                        printf("\033[31m[KASJER %d] obsłużył wszystkich\033[0m\n\n", cashier_id);

                        pthread_exit(NULL);//koniec wątku kasjera
                    }else{
                        
                    }
                }

                //  usleep(10000); // Jeśli nie ma wiadomości, kasjer czeka
                continue;
           }
            else if (errno == EINTR) {
            printf("msgrcv przerwane przez sygnał, ponawiam próbę...\n");
            continue;
            }
            perror("Błąd odbierania komunikatu kasjer");
            exit(1);
        }

        printf("Kasjer %d obsługuje klienta o PID = %d\n", cashier_id, message.customer_pid);

        // Czas obsługi klienta - losowy
        // int czas =generate_random_time(3.0, 5.0); 
    //     // printf("czas : %d\n\n",czas);
    //         int min_time = 500000;  // 0.5 sekundy w mikrosekundach
    // int max_time = 1000000; // 1 sekunda w mikrosekundach
    // int random_time = min_time + rand() % (max_time - min_time + 1);

    // // Uśpienie na losowy czas
    // usleep(random_time);

        // Wysłanie odpowiedzi do klienta
        message.mtype = message.customer_pid;
        if (msgsnd(queue_id, &message, sizeof(message) - sizeof(long), 0) == -1) {
            perror("Błąd wysyłania komunikatu do klienta");
            exit(1);
        }
        printf("Kasjer %d zakończył obsługę klienta o PID = %d \n", cashier_id, message.customer_pid);
    }

    pthread_exit(NULL);

}

//koniec pracy kasjera gdy jest pożar
void handle_cashier_signal_fire(int sig) {
       int cashier_id = *((int*)pthread_getspecific(cashier_thread_key));
    if (cashier_id == -1) {
        fprintf(stderr, "Błąd: Brak przypisanego ID kasjera do wątku\n");
        exit(1); 
    }
    printf("DOSTALEM SYGNAL KASJER%d",cashier_id);
 
    // // // Zapisz czas początkowy
    // while (get_customer_count(shared_mem) > 0) {
    //     printf("Pozostali klienci w sklepie %d \n",get_customer_count(shared_mem));
    //     // fflush(stdin);
    //     // Czekaj, aż wszyscy klienci wyjdą
    //     // sleep(1);  // Oczekiwanie przez 1 sekundę
    // }

    // Kasjer kończy pracę
    pthread_exit(NULL);
}

//oczekiwanie na wyjście wszystkich kasjerów
void wait_for_cashiers(pthread_t* cashier_threads) {
    printf("ZOBACZMY CZY ZAMYKAMY KASY\n");
    void* status;
    int num_cashiers = get_current_cashiers();
      printf("ZOBACZMY ilosc as %dCZY ZAMYKAMY KASY\n",num_cashiers);
    for (int i = 0; i < num_cashiers; i++) { //zmienione na < z <=
        pthread_t cashier_thread = get_cashier_thread(cashier_threads, i);

        // Sprawdzamy, czy wątek kasjera jest zainicjowany
        if (cashier_thread != 0) {
            int ret = pthread_join(cashier_thread, &status);  // Czeka na zakończenie wątku
            if (ret == 0) {
                     decrement_cashiers();
                printf("Wątek kasjera %d zakończył się z kodem: %ld, ttid: %lu\n", i+1, (long)status, cashier_thread);
            } else {
                // Obsługuje błąd w przypadku niepowodzenia w oczekiwaniu na wątek
                fprintf(stderr, "Błąd podczas oczekiwania na wątek kasjera %d: %d\n", i+1, ret);
            }
        }else{
            printf("NIE ZAINICJALIZOWANY WATEK\n");
        }
    }
    printf("Kasjerzy wyszli\n");
}

//czyszczenie kolejki komunikatów dla danego kasjera
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

//usuwanie wszystkich kolejek komunikatów (Wywoływane z poziomu manadzera);
void cleanAfterCashiers() {
    for (int i = 1; i <= MAX_CASHIERS; i++) { //od jedynki ponieważ kasjerzy mają id od 1 do 10
        cleanup_queue(i);
    }

    printf("Wyczyszczono\n");
}
