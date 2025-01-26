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


pthread_mutex_t pid_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex dla ochrony tablicy PID-ów
pid_t pids[MAX_CUSTOMERS];  // Tablica przechowująca PIDs procesów potomnych
int created_processes = 0;  // Licznik stworzonych procesów

extern SharedMemory* shared_mem;  // Dostęp do pamięci dzielonej



void destroy_pid_mutex(){
        // Próba zniszczenia mutexa
    int ret = pthread_mutex_destroy(&pid_mutex);
    if (ret != 0) {
        fprintf(stderr, "Błąd podczas niszczenia mutexa\n");
        exit(1);  // Zakończenie programu w przypadku błędu
    }else{
        printf("Semafor pid_mutex poprawnie usuniety\n");
    }
}

void* cleanup_processes(void* arg) {
    int status;
    pid_t finished_pid;

    while (1) {
        pthread_mutex_lock(&pid_mutex);

        // Jeśli nie ma klientów i flaga pożaru jest ustawiona, zakończ pętlę
        if (get_fire_handling_complete(shared_mem) && created_processes == 0) {
            pthread_mutex_unlock(&pid_mutex);
            break;
        }

        int current_created_processes = created_processes; // Kopia liczby procesów
        pid_t local_pids[MAX_CUSTOMERS];                  // Kopia tablicy PID
        for (int i = 0; i < created_processes; i++) {
            local_pids[i] = pids[i];
        }

        pthread_mutex_unlock(&pid_mutex);

        // Przetwarzanie procesów poza mutexem
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

                printf("Klient %d zakończył działanie.\n", finished_pid);
            }
        }

        // Jeśli flaga pożaru jest ustawiona, zabij wszystkie pozostałe procesy
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
                printf("Klient %d został zabity (SIGTERM).\n", local_pids[i]);
            }
        }
    }

    printf("Koniec czyszczenia procesów.\n");
    return NULL;
}



//inicjalizacja semafora dla klientów na MAX_CUSTOMER
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
    printf("Semafor customer utwozony poprawnie\n");
}


int safe_sem_wait() {
    sem_t* semaphore = get_semaphore_customer(); 


    while (sem_wait(semaphore) == -1) {
        if (errno == EINTR) {  // Obsługa przerwania przez sygnał

            continue;  // Jeśli przerwanie było inne, próbuj ponownie
        }

        // Logowanie błędu w przypadku innego problemu z semaforem
        perror("Błąd podczas oczekiwania na semafor");
        return -1;
    }

    return 0;  // Semafor uzyskany poprawnie
}

int safe_sem_post() {
    sem_t* semaphore = get_semaphore_customer();


    while (sem_post(semaphore) == -1) {
        if (errno == EINTR) {  // Przerwanie przez sygnał
            // Logowanie próby ponownego zwolnienia semafora
            continue;  // Spróbuj ponownie
        } else {
            // Logowanie błędu, jeśli semafor nie może zostać zwolniony
            perror("Błąd podczas zwalniania semafora");
            return -1;
        }
    }

    return 0;  // Semafor zwolniony pomyślnie
}




int generate_random_time(float min_seconds, float max_seconds) {
    if (min_seconds < 0 || max_seconds < 0 || min_seconds > max_seconds) {
        fprintf(stderr, "Błędne wartości zakresu czasu. MIN >= 0, MAX >= 0, a MIN <= MAX.\n");
        return -1;
    }

    // Zamiana sekund na milisekundy (1 sekunda = 1000 milisekund)
    int min_milliseconds = (int)(min_seconds * 1000);  // 1 sekunda = 1000 milisekund
    int max_milliseconds = (int)(max_seconds * 1000);  // 1 sekunda = 1000 milisekund


    // Losowy czas w milisekundach
    int random_milliseconds = (rand() % (max_milliseconds - min_milliseconds + 1)) + min_milliseconds;

    return random_milliseconds;  // Zwracamy czas w milisekundach
}


//zwracanie semafora istniejącego lub go stwórz
sem_t* get_semaphore_customer() {
    sem_t* semaphore = sem_open(SEMAPHORE_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, MAX_CUSTOMERS);
    if (semaphore == SEM_FAILED) {
        if (errno == EEXIST) {
            semaphore = sem_open(SEMAPHORE_NAME, 0); // Otwieramy istniejący semafor
            if (semaphore == SEM_FAILED) {
                perror("Błąd otwierania semafora customer");
                exit(1);  // Zakończenie programu w przypadku błędu
            }
        } else {
            perror("Błąd otwierania semafora customer");
            exit(1);  // Zakończenie programu w przypadku błędu
        }
    }
    return semaphore;
}

void destroy_semaphore_customer() {
    sem_t* semaphore = get_semaphore_customer();
    sem_close(semaphore);  // Close the semaphore
    if (sem_unlink(SEMAPHORE_NAME) == -1) {
        perror("Błąd usuwania semafora");
    } else {
        printf("Semafor został poprawnie usunięty customer.\n");
    }
}


void log_semaphore_wait(pid_t pid, const char* semaphore_name, const char* action) {
    printf("Proces %d: %s na semaforze %s\n", pid, action, semaphore_name);
}

