#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>   
#include <errno.h>   
#include "creator_customer.h"
#include "shared_memory.h"
#include "manager_cashiers.h"
#include "firefighter.h"
#include "cashier.h"

SharedMemory* shared_mem = NULL; // Wskaźnik do pamięci dzielonej
pthread_t monitor_thread;  // Wątek menedżera kasjerów
pthread_t customer_thread; // Wątek obsługujący klientów
pthread_t firefighter_thread; // Wątek strażaka
pthread_t cleanup_thread;  // Wątek czyszczący zakończone procesy


// Zewnętrzne deklaracje zmiennych
extern pthread_t cashier_threads[MAX_CASHIERS]; // Tablica wątków kasjerów
extern pthread_mutex_t pid_mutex;  // Mutex dla tablicy PID-ów klientów
extern pid_t pids[MAX_CUSTOMERS];  // Tablica PID-ów klientów
extern int created_processes ; // Licznik utworzonych procesów klientów

/**
 * @brief Główna funkcja programu, odpowiedzialna za zarządzanie działaniem supermarketu.
 * 
 * @details Funkcja inicjalizuje wszystkie wymagane zasoby, takie jak pamięć dzielona, semafory
 * oraz wątki pomocnicze (manager kasjerów, strażak, wątek czyszczący). Uruchamia również
 * główną pętlę tworzącą procesy klientów. Po ustawieniu flagi pożaru funkcja odpowiedzialna
 * jest za bezpieczne zamknięcie supermarketu oraz sprzątnięcie wszystkich zasobów.
 * 
 * @return Zwraca 0 w przypadku powodzenia.
 */
int main() {
    srand(time(NULL));  // Inicjalizacja generatora liczb pseudolosowych

    set_process_group();                    // Ustawienie grupy procesów
    init_semaphore_customer();              // Inicjalizacja semafora klientów
    shared_mem = init_shared_memory();      // Inicjalizacja pamięci dzielonej

    // Rejestracja handlera sygnału SIGINT (Ctrl+C)
    signal(SIGINT, onFireSignalHandler);

    // Rejestracja handlera sygnału SIGUSR1 Strażak
    signal(SIGUSR1, onFireSignalHandlerForFirefighter);

    // Inicjalizacja wątku menedżera kasjerów
    init_manager(&monitor_thread);

    // Inicjalizacja wątku strażaka
    init_firefighter(&firefighter_thread);

    // Tworzymy wątek do czyszczenia zakończonych procesów
    init_cleanup_thread(&cleanup_thread);

    // Główna pętla obsługująca generowanie klientów
    while (!get_fire_flag(shared_mem)) {
  
        if (safe_sem_wait() == -1) {        // Oczekiwanie na miejsce do utworzenia nowego klienta
            perror("Błąd podczas oczekiwania na semafor");
            break;
        }
        increment_customer_count(shared_mem);

        pid_t pid = fork();                 // Tworzenie nowego procesu klienta
        if (pid < 0) {
            perror("Fork failed, błędne wywołanie fork");
            safe_sem_post();                // Zwolnienie semafora w przypadku błędu
        } else if (pid == 0) {
            // Kod wykonywany przez proces potomny (klient)
            if (execl("./src/customer", "customer", (char *)NULL) == -1) {
                perror("Błąd wykonania programu customer");
                exit(1);
            }
        } else {
            // Kod wykonywany przez proces główny (supermarket)
            pthread_mutex_lock(&pid_mutex);
            pids[created_processes++] = pid; // Zapisanie PID-u klienta
            pthread_mutex_unlock(&pid_mutex);
        }

        // Generowanie losowego czasu oczekiwania między klientami
        int stay_time_in_microseconds = generate_random_time(MIN_TIME_TO_CLIENT, MAX_TIME_TO_CLIENT);
        usleep(stay_time_in_microseconds);
    }

    // Procedura zamknięcia po ustawieniu flagi pożaru

    set_fire_handling_complete(shared_mem,1);   // Ustawienie flagi końca generowania klientów
    send_signal_to_firefighter(SIGQUIT);        // Wysyłanie sygnału SIGQUIT do strażaka
    pthread_join(cleanup_thread, NULL);         // Czekanie na zakończenie wątku czyszczącego
    
    send_signal_to_cashiers(SIGHUP);            // Wysyłanie sygnału do kasjerów, aby zakończyli pracę
    wait_for_cashiers(cashier_threads,get_current_cashiers()); // Czekanie na zakończenie wątków kasjerów
    wait_for_manager(monitor_thread);           // Czekanie na zakończenie wątku menedżera

    wait_for_firefighter(firefighter_thread);   // Czekanie na zakończenie wątku strażaka

    cleanAfterCashiers();                       // Czyszczenie kolejek kasjerów
    cleanup_shared_memory(shared_mem);          // Usuwanie pamięci dzielonej i jej semaforów
    destroy_semaphore_customer();               // Usuwanie semafora klientów
    destroy_mutex();                            // Usuwanie mutexów
    destroy_pid_mutex();                        // Usuwanie mutexa PID-ów

    printf("KONIEC\n");
    return 0;
}

/**
 * @brief Wysyła sygnał do wątku menedżera kasjerów.
 * 
 * @param signal Sygnał, który zostanie wysłany (np. `SIGQUIT` lub `SIGHUP`).
 * 
 * @details Funkcja wykorzystuje `pthread_kill` do wysłania sygnału do wątku menedżera kasjerów.
 * Jeśli wysyłanie sygnału się nie powiedzie, wypisuje komunikat o błędzie.
 */
void send_signal_to_manager(int signal) {
    if (pthread_kill(monitor_thread, signal) != 0) {
            perror("Błąd wysyłania sygnału do kasjera");
    }
}

/**
 * @brief Wysyła sygnał do wątku klientów.
 * 
 * @param signal Sygnał, który zostanie wysłany (np. `SIGQUIT`).
 * 
 * @details Funkcja wysyła określony sygnał do wątku klientów za pomocą `pthread_kill`.
 * W przypadku błędu generuje komunikat diagnostyczny.
 */
void send_signal_to_customers(int signal) {
    if (pthread_kill(customer_thread, signal) != 0) {
            perror("Błąd wysyłania sygnału do kasjera");
    }
}

/**
 * @brief Wysyła sygnał do wątku strażaka.
 * 
 * @param signal Sygnał, który zostanie wysłany (np. `SIGQUIT`).
 * 
 * @details Funkcja korzysta z `pthread_kill`, aby wysłać sygnał do strażaka.
 * Jeśli wystąpi błąd (np. strażak już zakończył działanie), funkcja wypisuje komunikat o błędzie.
 */
void send_signal_to_firefighter(int signal) {
    int err = pthread_kill(firefighter_thread, 0); // Check if the thread is still alive
    if (err == 0) {
        // Thread is alive, send the specified signal
        if (pthread_kill(firefighter_thread, signal) != 0) {
            perror("Błąd wysyłania sygnału do strażaka");
        } else {
            printf("Sygnał %d wysłany do strażaka.\n", signal);
        }
    }
}


/**
 * @brief Ustawia bieżący proces jako lidera grupy procesów.
 * 
 * @details Funkcja ustawia bieżący proces (PID) jako lidera grupy procesów, co umożliwia
 * strażakowi i innym procesom wysyłanie sygnałów do całej grupy.
 * 
 * @note Jeśli `setpgid` zwróci błąd, program zostaje zakończony.
 */
void set_process_group() {
    pid_t pid = getpid();
     if (setpgid(pid, pid) == -1) {// Ustawienie głównego procesu jako lidera grupy procesów (po to żeby strażak mógł wysłać poprawnie sygnały)
        perror("Błąd przy ustawianiu grupy procesów");
        exit(1);  // Zakończenie programu w przypadku błędu
    } 
}

/**
 * @brief Obsługuje sygnał SIGINT (Ctrl+C) w celu rozpoczęcia procedury zamknięcia supermarketu.
 * 
 * @param signum Numer sygnału, który wywołał handler.
 * 
 * @details Funkcja ustawia flagę pożaru w pamięci dzielonej oraz uruchamia procedurę
 * odliczania do zamknięcia supermarketu. Jeśli flaga pożaru jest już ustawiona, handler
 * nie wykonuje żadnych działań.
 */
void onFireSignalHandler(int signum) {

    if(get_fire_flag(shared_mem)==1){
        return;
    }
    set_fire_flag(shared_mem,1); 
    countdown_to_exit();
}



/**
 * @brief Obsługuje sygnał SIGUSR w celu rozpoczęcia procedury zamknięcia supermarketu na strażaka.
 * 
 * @param signum Numer sygnału, który wywołał handler.
 * 
 * @details Funkcja ustawia flagę pożaru w pamięci dzielonej oraz uruchamia procedurę
 * odliczania do zamknięcia supermarketu. Jeśli flaga pożaru jest już ustawiona, handler
 * nie wykonuje żadnych działań.
 */
void onFireSignalHandlerForFirefighter(int signum) {

    if(get_fire_flag(shared_mem)==1){
        return;
    }
    set_fire_flag(shared_mem,1); 
    countdown_to_exit();
    send_signal_to_cashiers(SIGHUP);  
}

