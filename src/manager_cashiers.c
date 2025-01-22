#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <sys/msg.h>  // Dla funkcji msgrcv i msgsnd
// #include "main.h" //Dla zmiennej MIN_CASHIERS
#include "customer.h"
#include "shared_memory.h"
#include "manager_cashiers.h"
#include "cashier.h"

extern SharedMemory* shared_mem; 

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t cashier_threads[MAX_CASHIERS];
int cashier_ids[MAX_CASHIERS];
int current_cashiers = 0;  // Liczba kasjerów działających

extern pthread_mutex_t customers_mutex;
extern pthread_cond_t customers_cond;


void send_signal_to_cashiers(int signal) {
    int sum_cash = get_current_cashiers();
    for (int i = 0; i < sum_cash; i++) {
        pthread_t cashier_thread = get_cashier_thread(cashier_threads, i); 
        if (pthread_kill(cashier_thread, signal) != 0) {   //wysylamy sygnal do kasjera aby zakończył pracę
            perror("Error sending signal to cashier thread");
        }
    }
}

// Wyyłamy sygnał SIGHUP do kasjerów, aby ci zakończyli swoją pracę
void fire_sigTermHandler(int signum) {   
    send_signal_to_cashiers(SIGHUP); //wysyłamy sygnał
    wait_for_cashiers(cashier_threads, get_current_cashiers()); //czekamy na zakończenie 
    cleanAfterCashiers();  // Sprzątanie po kasjerach kolejek komunikatów
    pthread_exit(NULL);  // Kasjer kończy pracę
}

void* manage_customers(void* arg) {
    if (signal(SIGTERM, fire_sigTermHandler) == SIG_ERR) {//łapanie sygnału o pożarze
        perror("Błąd przy ustawianiu handlera dla SIGTERM");
        exit(1); 
    } 
    create_initial_cashiers(cashier_threads, cashier_ids);

    while (1) {
       
        int num_customers = get_customers_in_shop();

        int required_cashiers = (num_customers + MIN_PEOPLE_FOR_CASHIER - 1) / MIN_PEOPLE_FOR_CASHIER;

        //jeśli liczba wymaganych kasjerów jest mniejsza niż minimalna liczba wymagana wtedy zakładamy że wymagani kasjerzy to wartość min
        if (required_cashiers < MIN_CASHIERS) {
            required_cashiers = MIN_CASHIERS;
        }


        // Dodaj nowych kasjerów
        if (num_customers < required_cashiers && get_current_cashiers() < MAX_CASHIERS) {
                    int new_cashier_id = get_current_cashiers() + 1;
                    set_cashier_id(cashier_ids, get_current_cashiers(), new_cashier_id);

                    pthread_t cashier_thread;
                    create_cashier(&cashier_thread, &cashier_ids[get_current_cashiers()]);
                    set_cashier_thread(cashier_threads, get_current_cashiers(), cashier_thread);

                    increment_cashiers();
                    increment_active_cashiers(shared_mem);

                    printf("\033[1;32m[KASJER %d] OTWIERANIE, Obecny zakres kasjerów : 1 - %d\033[0m\n\n", get_current_cashiers(), get_active_cashiers(shared_mem));
        }

        
        // Zmniejsz liczbę kasjerów
        if (num_customers < MIN_PEOPLE_FOR_CASHIER * (get_current_cashiers() - 1) && get_current_cashiers() > MIN_CASHIERS) {

                    decrement_active_cashiers(shared_mem);
                    int cashier_to_remove = get_current_cashiers(); 
                    pthread_t cashier_thread = get_cashier_thread(cashier_threads, cashier_to_remove - 1);

                    printf("\033[38;5;196m[KASJER %d] już nie przyjmuje więcej klientów - Wątek: %ld\033[0m\n", cashier_to_remove, cashier_thread);

                    if (pthread_kill(cashier_thread, SIGUSR1) != 0) {
                        perror("Błąd podczas wysyłania sygnału do kasjera");
                        continue;
                    }

                    void* status = NULL;
                    int ret = pthread_join(cashier_thread, &status);
                    if (ret == 0) {
                        printf("\033[38;5;196m[KAJSER %d] kasa już zakończyła pracę , kod zakończenia: %ld\033[0m\n", cashier_to_remove, (long)status);
                    } else {
                        perror("Błąd podczas oczekiwania na zakończenie wątku kasjera");
                    }

                    decrement_cashiers();
        }

    }

    return NULL;
}

void create_initial_cashiers(pthread_t* cashier_threads, int* cashier_ids) {
    for (int i = 0; i < MIN_CASHIERS; i++) {
        set_cashier_id(cashier_ids, i, i + 1);  // Przypisanie ID kasjera
        int* cashier_id = get_cashier_id_pointer(cashier_ids, i);  // Pobranie ID kasjera

        pthread_t cashier_thread;
        create_cashier(&cashier_thread, cashier_id);  // Tworzymy wątek kasjera
        set_cashier_thread(cashier_threads, i, cashier_thread);  // Ustawiamy wątek kasjera w tablicy (funkcja z mutexem)

        increment_cashiers();  // Zwiększamy liczbę kasjerów
        increment_active_cashiers(shared_mem);
    }
    printf("Utworzono minimalną liczbę kasjerów: %d\n", MIN_CASHIERS);
}

// Tworzenie wątku menedżera kasjerów, który będzie monitorował liczbę klientów
void init_manager(pthread_t* manager_thread) {
    if (pthread_create(manager_thread, NULL, manage_customers, NULL) != 0) {
        perror("Błąd tworzenia wątku menedżera kasjerów");
        exit(1);
    }
    printf("Wątek menedżera kasjerów utworzony, id wątku: %ld\n", *manager_thread);
}

// Czekanie na zakończenie wątku menedżera
void wait_for_manager(pthread_t manager_thread) { 
    // Czekamy na zakończenie wątku menedżera
    int ret = pthread_join(manager_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "Błąd podczas czekania na wątek menedżera\n");
        exit(1);  // Zakończenie programu w przypadku błędu
    }

    // Próba zniszczenia mutexa
    ret = pthread_mutex_destroy(&mutex);
    if (ret != 0) {
        fprintf(stderr, "Błąd podczas niszczenia mutexa\n");
        exit(1);  // Zakończenie programu w przypadku błędu
    }

    printf("Menadżer zakończył działanie.\n");
}

//zwiększanie liczby kasjerów z mutexem
void increment_cashiers() {
    int ret = pthread_mutex_lock(&mutex);
    if (ret != 0) {
        perror("Błąd podczas blokowania mutexa w increment_cashiers");
        exit(1);
    }

    current_cashiers++;

    ret = pthread_mutex_unlock(&mutex);
    if (ret != 0) {
        perror("Błąd podczas zwalniania mutexa w increment_cashiers");
        exit(1);
    }
}

//zmniejszanie liczby kasjerów z mutexem
void decrement_cashiers() {
    int ret = pthread_mutex_lock(&mutex);
    if (ret != 0) {
        perror("Błąd podczas blokowania mutexa w decrement_cashiers");
        exit(1);
    }
    current_cashiers--;

    ret = pthread_mutex_unlock(&mutex);
    if (ret != 0) {
        perror("Błąd podczas zwalniania mutexa w decrement_cashiers");
        exit(1);
    }
}

//zwracanie obecnej liczby kasjerów z mutexem
int get_current_cashiers() {
    int count;

    int ret = pthread_mutex_lock(&mutex);
    if (ret != 0) {
        perror("Błąd podczas blokowania mutexa w get_current_cashiers");
        exit(1); 
    }
    count = current_cashiers;

    ret = pthread_mutex_unlock(&mutex);
    if (ret != 0) {
        perror("Błąd podczas zwalniania mutexa w get_current_cashiers");
        exit(1); 
    }
    return count;
}

pthread_t get_cashier_thread(pthread_t* cashier_threads, int index) {
    pthread_t thread;
    int ret = pthread_mutex_lock(&mutex);  // Zabezpieczenie przed równoczesnym dostępem do tablicy
    if (ret != 0) {
        perror("Błąd podczas blokowania mutexa w get_cashier_thread");
        exit(1);  
    }
    thread = cashier_threads[index];
    ret = pthread_mutex_unlock(&mutex);  // Zwolnienie mutexu
    if (ret != 0) {
        perror("Błąd podczas zwalniania mutexa w get_cashier_thread");
        exit(1);  
    }
    return thread;
}

// Funkcja przypisująca wątek kasjera do tablicy cashier_threads
void set_cashier_thread(pthread_t* cashier_threads, int index, pthread_t thread) {
    int ret = pthread_mutex_lock(&mutex);
    if (ret != 0) {
        perror("Błąd podczas blokowania mutexa w get_cashier_thread");
        exit(1);  
    }
    cashier_threads[index] = thread;  // Ustawienie wątku kasjera w tablicy
    ret = pthread_mutex_unlock(&mutex);  // Zwolnienie mutexu
    if (ret != 0) {
        perror("Błąd podczas zwalniania mutexa w get_cashier_thread");
        exit(1);  
    } 
}

//zwracanie id kasjera
int get_cashier_id(int* cashier_ids, int index) {
    int cashier_id;
    int ret = pthread_mutex_lock(&mutex);  
    if (ret != 0) {
        perror("Error while locking mutex in get_cashier_id");
        exit(1);
    }
    cashier_id = cashier_ids[index];  // Pobieramy ID kasjera z tablicy
    ret = pthread_mutex_unlock(&mutex);
    if (ret != 0) {
        perror("Error while unlocking mutex in get_cashier_id");
        exit(1); 
    }
    return cashier_id; 
}

//ustawienie id kasjera w tablicy cashiers_ids
void set_cashier_id(int* cashier_ids, int index, int id) {
    int ret = pthread_mutex_lock(&mutex);  
    if (ret != 0) {
        perror("Error while locking mutex in get_cashier_id");
        exit(1);
    }
    cashier_ids[index] = id;  // Ustawienie ID kasjera w tablicy
    ret = pthread_mutex_unlock(&mutex); 
    if (ret != 0) {
        perror("Błąd podczas zwalniania mutexa w get_cashier_thread");
        exit(1);  
    }  
}

// Funkcja zwracająca wskaźnik do elementu tablicy cashier_ids
int* get_cashier_id_pointer(int* cashier_ids, int index) {
    int* cashier_id = NULL;
    int ret = pthread_mutex_lock(&mutex);  
    if (ret != 0) {
        perror("Error while locking mutex in get_cashier_id");
        exit(1);
    }  
    cashier_id = &cashier_ids[index];// Zwracamy wskaźnik do odpowiedniego elementu
    ret = pthread_mutex_unlock(&mutex);  
    if (ret != 0) {
        perror("Błąd podczas zwalniania mutexa w get_cashier_thread");
        exit(1);  
    } 
    return cashier_id;
}

// Funkcja zwraca liczbę wiadomości o określonym mtype (cashier_id) w danej kolejce
int get_message_count_for_cashier(int queue_id) {
    struct msqid_ds queue_info;  // Struktura do przechowywania informacji o kolejce

    // Uzyskujemy informacje o kolejce, ale jej nie usuwamy
    if (msgctl(queue_id, IPC_STAT, &queue_info) == -1) {
        perror("Błąd podczas pobierania informacji o kolejce");
        return -1;  // Jeśli pobranie informacji o kolejce się nie udało, zwracamy -1
    }

    return queue_info.msg_qnum;
}

// Funkcja zwracająca numer kasjera z najmniejszą liczbą osób w kolejce
int select_cashier_with_fewest_people(SharedMemory* shared_mem) {
    int active_cashiers = get_active_cashiers(shared_mem); 

    if (active_cashiers <= 0) {
        printf("Brak aktywnych kasjerów.\n");
        return -1; 
    }

    int min_cashier_id = -1;
    int min_message_count = INT_MAX; 

    // Przeszukujemy wszystkie aktywne kasjery
    for (int i = 0; i < active_cashiers; i++) {
        int queue_id = get_queue_id(shared_mem, i + 1);  // Pobieramy queue_id dla kasjera
        if (queue_id != -1) {  // Sprawdzamy, czy kolejka jest przypisana

            int message_count = get_message_count_for_cashier(queue_id);  // Liczymy wiadomości w kolejce dla kasjera

            // Jeśli liczba wiadomości w tej kolejce jest mniejsza, zapisz ten kasjer
            if (message_count < min_message_count) {
                min_message_count = message_count;
                min_cashier_id = i + 1;  // Numer kasjera z najmniejszą liczbą wiadomości
            }
        }
    }
    if (min_cashier_id == -1) {
        printf("Nie znaleziono żadnej ważnej kolejki.\n");
        return -1;  // Brak odpowiednich kolejek
    }
    return min_cashier_id;
}