#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <sys/msg.h>
#include "shared_memory.h"
#include "manager_cashiers.h"
#include "cashier.h"

extern SharedMemory* shared_mem; 

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t cashier_threads[MAX_CASHIERS];
int cashier_ids[MAX_CASHIERS];
int current_cashiers = 0;  
int terminate_flags[MAX_CASHIERS] = {0};  

/**
 * @brief Wysyła sygnał do wszystkich aktywnych kasjerów.
 * 
 * @param signal Sygnał, który ma zostać wysłany (np. `SIGHUP` lub `SIGQUIT`).
 */
void send_signal_to_cashiers(int signal) {
    int sum_cash = get_current_cashiers();
    for (int i = 0; i < sum_cash; i++) {
        pthread_t cashier_thread = get_cashier_thread(cashier_threads, i); 
        if (pthread_kill(cashier_thread, signal) != 0) {  
            // perror("Error sending signal to cashier thread");
        }
    }
}

/**
 * @brief Zarządza kasjerami w zależności od liczby klientów.
 * 
 * @details Dynamicznie dodaje lub usuwa kasjerów na podstawie liczby klientów.
 * Jeśli liczba klientów spada poniżej wymaganego progu, nadmiarowi kasjerzy są zwalniani.
 */
void* manage_customers(void* arg) {
    create_initial_cashiers(cashier_threads, cashier_ids);

    while (!get_fire_flag(shared_mem)) {
        int num_customers = get_customer_count(shared_mem);
        int required_cashiers = (num_customers + MIN_PEOPLE_FOR_CASHIER - 1) / MIN_PEOPLE_FOR_CASHIER;
       
        //Jeśli liczba wymaganych kasjerów jest mniejsza niż minimalna liczba wymagana wtedy zakładamy że wymagani kasjerzy to wartość min
        if (required_cashiers < MIN_CASHIERS) {
            required_cashiers = MIN_CASHIERS;
        }

        // Dodawanie kasjerów
        while ( get_current_cashiers() < required_cashiers && get_current_cashiers() < MAX_CASHIERS && !get_fire_flag(shared_mem)) {
            int new_cashier_id = get_current_cashiers() + 1;
            set_cashier_id(cashier_ids, get_current_cashiers(), new_cashier_id);

            pthread_t cashier_thread;
            create_cashier(&cashier_thread, &cashier_ids[get_current_cashiers()]);
            set_cashier_thread(cashier_threads, get_current_cashiers(), cashier_thread);
            
            increment_cashiers();
            increment_active_cashiers(shared_mem);

            printf("\033[1;32m[KASJER %d] OTWIERANIE, watek %ld , Obecny zakres kasjerów : 1 - %d\033[0m\n\n",get_current_cashiers(),cashier_thread, get_active_cashiers(shared_mem));
            usleep(1000);
        }

        // Usuwanie zbędnych kasjerów
        while (get_customer_count(shared_mem)  < MIN_PEOPLE_FOR_CASHIER * ( get_current_cashiers()  - 1) &&  get_current_cashiers() > MIN_CASHIERS && !get_fire_flag(shared_mem)) {
            decrement_active_cashiers(shared_mem);
            int cashier_to_remove =  get_current_cashiers() ; 
            pthread_t cashier_thread = get_cashier_thread(cashier_threads, cashier_to_remove - 1);
            
            // Sekcja krytyczna dla zmiany wartości flag kasjerów o końcu pracy
            int ret = pthread_mutex_lock(&mutex);
            if (ret != 0) {
                perror("Błąd podczas blokowania mutexa w closeCashier");
                pthread_exit(NULL);
            }
            terminate_flags[cashier_to_remove - 1]=1;
            ret = pthread_mutex_unlock(&mutex);
            if (ret != 0) {
                perror("Błąd podczas zwalniania mutexa w closeCashier");
                pthread_exit(NULL);
            }

            printf("\033[38;5;196m[KASJER %d] już nie przyjmuje więcej klientów - Wątek: %ld\033[0m\n", cashier_to_remove, cashier_thread);

            decrement_cashiers();

            void* status = NULL;
            int ret2 = pthread_join(cashier_thread, &status);
            if (ret2 == 0) {
                printf("\033[38;5;196m[KAJSER %d] kasa już zakończyła pracę , kod zakończenia: %ld\033[0m\n", cashier_to_remove, (long)status);
            } else {
                 perror("Błąd podczas oczekiwania na zakończenie wątku kasjera");
            }  
            usleep(1000);                    
        }
        usleep(10000);
    }
    return NULL;
}

/**
 * @brief Tworzy minimalną wymaganą liczbę kasjerów na start.
 * 
 * @param cashier_threads Tablica przechowująca wątki kasjerów.
 * @param cashier_ids Tablica przechowująca identyfikatory kasjerów.
 */
void create_initial_cashiers(pthread_t* cashier_threads, int* cashier_ids) {
    for (int i = 0; i < MIN_CASHIERS; i++) {
        set_cashier_id(cashier_ids, i, i + 1);                      // Przypisanie ID kasjera
        int* cashier_id = get_cashier_id_pointer(cashier_ids, i);   // Pobranie ID kasjera

        pthread_t cashier_thread;
        create_cashier(&cashier_thread, cashier_id);                // Tworzymy wątek kasjera
        set_cashier_thread(cashier_threads, i, cashier_thread);     // Ustawiamy wątek kasjera w tablicy (funkcja z mutexem)

        printf("[SET CASHIERS] Stworzono wątek kasjera  %ld, a w tablicy jest %ld\n",cashier_thread,get_cashier_thread(cashier_threads, i));

        increment_cashiers(); 
        increment_active_cashiers(shared_mem);
    }
    printf("Utworzono minimalną liczbę kasjerów: %d\n", MIN_CASHIERS);
}

/**
 * @brief Inicjalizuje wątek menedżera kasjerów.
 * 
 * @param manager_thread Wskaźnik do wątku menedżera.
 */
void init_manager(pthread_t* manager_thread) {
    if (pthread_create(manager_thread, NULL, manage_customers, NULL) != 0) {
        perror("Błąd tworzenia wątku menedżera kasjerów");
        exit(1);
    }
    printf("Wątek menedżera kasjerów utworzony, id wątku: %ld\n", *manager_thread);
}

/**
 * @brief Czeka na zakończenie wątku menedżera.
 * 
 * @param manager_thread Identyfikator wątku menedżera.
 */
void wait_for_manager(pthread_t manager_thread) { 
    int ret = pthread_join(manager_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "Błąd podczas czekania na wątek menedżera\n");
    }else{
        printf("Menadżer zakończył działanie.\n");
    }
}

/**
 * @brief Niszczy mutex używany do synchronizacji.
 */
void destroy_mutex(){
    int ret = pthread_mutex_destroy(&mutex);
    if (ret != 0) {
        fprintf(stderr, "Błąd podczas niszczenia mutexa\n");
        exit(1); 
    }else{
        printf("Semafor pid_mutex poprawnie usuniety\n");
    }
}

/**
 * @brief Zwiększa liczbę aktualnie aktywnych kasjerów.
 * 
 * @details Funkcja używa mutexa w celu zapewnienia synchronizacji podczas modyfikacji 
 * zmiennej `current_cashiers`. Zwiększa jej wartość o 1.
 */
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

/**
 * @brief Zmniejsza liczbę aktualnie aktywnych kasjerów.
 * 
 * @details Funkcja używa mutexa w celu zapewnienia synchronizacji podczas modyfikacji 
 * zmiennej `current_cashiers`. Zmniejsza jej wartość o 1.
 */
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

/**
 * @brief Zwraca aktualną liczbę kasjerów.
 * 
 * @return Liczba aktywnych kasjerów.
 * 
 * @details Funkcja używa mutexa do zapewnienia bezpieczeństwa podczas odczytu zmiennej 
 * `current_cashiers`.
 */
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

/**
 * @brief Zwraca wątek kasjera z tablicy cashier_threads.
 * 
 * @param cashier_threads Tablica wątków kasjerów.
 * @param index Indeks kasjera w tablicy.
 * @return Identyfikator wątku kasjera.
 */
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

/**
 * @brief Przypisuje wątek kasjera do tablicy cashier_threads.
 * 
 * @param cashier_threads Tablica wątków kasjerów.
 * @param index Indeks kasjera w tablicy.
 * @param thread Identyfikator wątku kasjera.
 */
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

/**
 * @brief Zwraca identyfikator kasjera z tablicy cashier_ids.
 * 
 * @param cashier_ids Tablica identyfikatorów kasjerów.
 * @param index Indeks kasjera w tablicy.
 * @return Identyfikator kasjera.
 */
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

/**
 * @brief Przypisuje identyfikator kasjera do tablicy cashier_ids.
 * 
 * @param cashier_ids Tablica identyfikatorów kasjerów.
 * @param index Indeks kasjera w tablicy.
 * @param id Identyfikator kasjera do przypisania.
 */
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

/**
 * @brief Zwraca wskaźnik do identyfikatora kasjera z tablicy cashier_ids.
 * 
 * @param cashier_ids Tablica identyfikatorów kasjerów.
 * @param index Indeks kasjera w tablicy.
 * @return Wskaźnik do identyfikatora kasjera.
 */
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

/**
 * @brief Zwraca liczbę wiadomości w kolejce przypisanej do kasjera.
 * 
 * @param queue_id Identyfikator kolejki komunikatów.
 * @return Liczba wiadomości w kolejce lub -1 w przypadku błędu.
 */
int get_message_count_for_cashier(int queue_id) {
    struct msqid_ds queue_info;  // Struktura do przechowywania informacji o kolejce

    // Uzyskujemy informacje o kolejce, ale jej nie usuwamy
    if (msgctl(queue_id, IPC_STAT, &queue_info) == -1) {
        perror("Błąd podczas pobierania informacji o kolejce");
        return -1; 
    }

    return queue_info.msg_qnum;
}

/**
 * @brief Wybiera kasjera z najmniejszą liczbą klientów w kolejce.
 * 
 * @param shared_mem Wskaźnik do pamięci dzielonej.
 * @return Identyfikator kasjera z najmniejszą liczbą klientów lub -1, jeśli brak aktywnych kasjerów.
 * 
 * @details Funkcja przeszukuje wszystkich aktywnych kasjerów w celu znalezienia tego, 
 * który ma najmniejszą liczbę wiadomości w swojej kolejce.
 */
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