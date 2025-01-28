#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include "shared_memory.h"
#include "main.h"
#include <fcntl.h>
#include <sys/stat.h>
#include "creator_customer.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/wait.h>

#define SEMAPHORE_NAME "/customer_semaphore"

pthread_mutex_t pid_mutex = PTHREAD_MUTEX_INITIALIZER;   // Mutex dla ochrony tablicy PID-ów
pid_t pids[MAX_CUSTOMERS];                               // Tablica przechowująca PIDs procesów potomnych
int created_processes = 0;                               // Licznik stworzonych procesów

extern SharedMemory* shared_mem;  // Wskaźnik do pamięci dzielonej

/**
 * @brief Niszczy mutex używany do ochrony tablicy PID-ów.
 * 
 * @details Funkcja zwalnia zasoby mutexa i kończy program w przypadku błędu.
 */
void destroy_pid_mutex(){
    int ret = pthread_mutex_destroy(&pid_mutex);
    if (ret != 0) {
        fprintf(stderr, "Błąd podczas niszczenia mutexa\n");
        exit(1);  // Zakończenie programu w przypadku błędu
    }else{
        printf("Semafor pid_mutex poprawnie usunięty.\n");
    }
}

/**
 * @brief Wątek czyszczący procesy klientów.
 * 
 * @details Funkcja sprawdza zakończone procesy klientów i usuwa ich PID-y z tablicy.
 * Jeśli ustawiona jest flaga pożaru, zabija wszystkie pozostałe procesy.
 */
void* cleanup_processes(void* arg) {
    int status;
    pid_t finished_pid;

    while (1) {
        pthread_mutex_lock(&pid_mutex);

        // Jeśli brak klientów i flaga pożaru jest ustawiona, zakończ
        if (get_fire_handling_complete(shared_mem) && created_processes == 0) {
            pthread_mutex_unlock(&pid_mutex);
            break;
        }

        // Tworzenie lokalnej kopii danych
        int current_created_processes = created_processes;  // Kopia liczby procesów
        pid_t local_pids[MAX_CUSTOMERS];                    // Kopia tablicy PID
        for (int i = 0; i < created_processes; i++) {
            local_pids[i] = pids[i];
        }

        pthread_mutex_unlock(&pid_mutex);

        // Sprawdzenie zakończonych procesów
        for (int i = 0; i < current_created_processes; i++) {
            finished_pid = waitpid(local_pids[i], &status, WNOHANG);
            if (finished_pid > 0) {
                pthread_mutex_lock(&pid_mutex);
                // Znajdź i usuń PID z tablicy
                for (int j = 0; j < created_processes; j++) {
                    if (pids[j] == finished_pid) {
                        for (int k = j; k < created_processes - 1; k++) {
                            pids[k] = pids[k + 1];
                        }
                        created_processes--;
                        decrement_customer_count(shared_mem);
                        safe_sem_post();
                        break;
                    }
                }
                pthread_mutex_unlock(&pid_mutex);

                printf("\t\t\t\tKlient %d zakończył działanie.\n", finished_pid);
            }
        }

        // Obsługa pożaru: zabicie pozostałych procesów
        if (get_fire_handling_complete(shared_mem)) {
            pthread_mutex_lock(&pid_mutex);
            current_created_processes = created_processes;
            for (int i = 0; i < created_processes; i++) {
                local_pids[i] = pids[i];
            }
            created_processes = 0;
            pthread_mutex_unlock(&pid_mutex);

            for (int i = 0; i < current_created_processes; i++) {
                kill(local_pids[i], SIGTERM);
                safe_sem_post();
                decrement_customer_count(shared_mem);
                waitpid(local_pids[i], &status, 0);
                printf("\t\t\t\tKlient %d zakończył działanie.\n", local_pids[i]);
            }
        }
        usleep(10000);
    }

    printf("Koniec czyszczenia procesów.\n");
    return NULL;
}

/**
 * @brief Tworzy wątek czyszczący procesy klientów.
 * 
 * @param cleanup_thread Wskaźnik do identyfikatora wątku.
 */
void init_cleanup_thread(pthread_t* cleanup_thread) {
    if (pthread_create(cleanup_thread, NULL, cleanup_processes, NULL) != 0) {
        perror("Błąd tworzenia wątku czyszczącego procesy");
        exit(1);
    }
    printf("Wątek czyszczący procesy klientów utworzony, id wątku: %ld\n", *cleanup_thread);
}

/**
 * @brief Inicjalizuje semafor dla klientów.
 * 
 * @details Tworzy nowy semafor lub otwiera istniejący, ustawia jego wartość na `MAX_CUSTOMERS`.
 */
void init_semaphore_customer() {
    sem_t* semaphore = sem_open(SEMAPHORE_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, MAX_CUSTOMERS); //S_IRUSR: Prawo do odczytu dla właściciela., S_IWUSR: Prawo do zapisu dla właściciela
    if (semaphore == SEM_FAILED) {
        if (errno == EEXIST) {
            printf("Semafor już istnieje, usuwam i tworzę nowy.\n");
            sem_unlink(SEMAPHORE_NAME);
            semaphore = sem_open(SEMAPHORE_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, MAX_CUSTOMERS);
        }
        if (semaphore == SEM_FAILED) {
            perror("Błąd inicjalizacji semafora");
            exit(1);
        }
    }
    printf("Semafor customer utworzony poprawnie\n");
}

/**
 * @brief Bezpieczne oczekiwanie na semafor.
 * 
 * @return 0 w przypadku powodzenia, -1 w przypadku błędu.
 */
int safe_sem_wait() {
    sem_t* semaphore = get_semaphore_customer(); 
    while (sem_wait(semaphore) == -1) {
        if (errno == EINTR) {  // Obsługa przerwania przez sygnał
            continue;  // Jeśli przerwanie było inne, próbuj ponownie
        }
        perror("Błąd podczas oczekiwania na semafor");
        return -1;
    }
    return 0; 
}

/**
 * @brief Bezpieczne zwalnianie semafora.
 * 
 * @return 0 w przypadku powodzenia, -1 w przypadku błędu.
 */
int safe_sem_post() {
    sem_t* semaphore = get_semaphore_customer();
    while (sem_post(semaphore) == -1) {
        if (errno == EINTR) {  // Przerwanie przez sygnał
            continue;  // Spróbuj ponownie
        } else {
            perror("Błąd podczas zwalniania semafora");
            return -1;
        }
    }
    return 0; 
}

/**
 * @brief Otwiera lub tworzy semafor dla klientów.
 * 
 * @return Wskaźnik do semafora.
 */
sem_t* get_semaphore_customer() {
    sem_t* semaphore = sem_open(SEMAPHORE_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, MAX_CUSTOMERS);
    if (semaphore == SEM_FAILED) {
        if (errno == EEXIST) {
            semaphore = sem_open(SEMAPHORE_NAME, 0); // Otwieramy istniejący semafor
            if (semaphore == SEM_FAILED) {
                perror("Błąd otwierania semafora customer");
                exit(1); 
            }
        } else {
            perror("Błąd otwierania semafora customer");
            exit(1);
        }
    }
    return semaphore;
}

/**
 * @brief Usuwa semafor klientów.
 */
void destroy_semaphore_customer() {
    sem_t* semaphore = get_semaphore_customer();
    sem_close(semaphore);  // Close the semaphore
    if (sem_unlink(SEMAPHORE_NAME) == -1) {
        perror("Błąd usuwania semafora");
    } else {
        printf("Semafor został poprawnie usunięty customer.\n");
    }
}

/**
 * @brief Konwertuje czas w sekundach na mikrosekundy.
 * 
 * @param seconds Czas w sekundach.
 * @return Czas w mikrosekundach lub -1 w przypadku błędu.
 */
int seconds_to_microseconds(float seconds) {
    if (seconds < 0) {
        fprintf(stderr, "Czas nie może być ujemny.\n");
        return -1; 
    }

    return (int)(seconds * 1000000);  // Zamiana sekund na mikrosekundy
}

/**
 * @brief Generuje losowy czas w zakresie.
 * 
 * @param min_seconds Minimalny czas w sekundach.
 * @param max_seconds Maksymalny czas w sekundach.
 * @return Losowy czas w mikrosekundach.
 */
int generate_random_time(float min_seconds, float max_seconds) {
    if (min_seconds < 0 || max_seconds < 0 || min_seconds > max_seconds) {
        fprintf(stderr, "Błędne wartości zakresu czasu. MIN >= 0, MAX >= 0, a MIN <= MAX.\n");
        return -1;
    }

    // Zamiana sekund na milisekundy
    int min_milliseconds = seconds_to_microseconds(min_seconds);
    int max_milliseconds = seconds_to_microseconds(max_seconds);

    if (min_milliseconds == -1 || max_milliseconds == -1) {
        return -1; // Błąd przy konwersji czasu
    }

    // Generowanie losowego czasu w zakresie
    int random_milliseconds = rand() % (max_milliseconds - min_milliseconds + 1) + min_milliseconds;

    return random_milliseconds;
}
