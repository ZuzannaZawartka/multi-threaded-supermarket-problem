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

void block_signal_SIGINT() {
    // Tworzenie maski sygnałów
    sigset_t block_set;
    sigemptyset(&block_set);            // Tworzenie pustej maski
    sigaddset(&block_set, SIGINT);      // Dodanie SIGINT do maski

    // Blokowanie sygnału SIGINT w tym wątku
    if (pthread_sigmask(SIG_BLOCK, &block_set, NULL) != 0) {
        perror("Nie udało się zablokować SIGINT w wątku");
        pthread_exit(NULL);
    }

    printf("SIGINT został zablokowany w tym wątku.\n");
}


void* cleanup_processes(void* arg) {

    // block_signal_SIGINT();


    int status;
    pid_t finished_pid;

    while (1) {
        // Sprawdzanie, czy zakończyć pętlę (brak klientów i flaga terminate_cleanup ustawiona)
        pthread_mutex_lock(&pid_mutex);  // Blokujemy mutex przed modyfikacją

        if (get_fire_flag(shared_mem) && created_processes == 0) {
            printf("Zakończenie czyszczenia procesów: Brak klientów i zakończenie flagi.\n");
            pthread_mutex_unlock(&pid_mutex);
            break;  // Zakończenie pętli, jeśli nie ma klientów i flaga jest ustawiona
        }

        // Sprawdzamy zakończone procesy i usuwamy ich PID z tablicy
        for (int i = 0; i < created_processes; i++) {
            finished_pid = waitpid(pids[i], &status, WNOHANG);  // Zwróci PID zakończonego procesu
            if (finished_pid > 0) {
                // Proces zakończony, usuwamy go z tablicy
                // printf("Proces %d zakończył działanie, usuwam z tablicy PID.\n", finished_pid);
                for (int j = i; j < created_processes - 1; j++) {
                    pids[j] = pids[j + 1];  // Przesuwamy elementy w tablicy
                }
                created_processes--;  // Zmniejszamy licznik procesów
                decrement_customer_count(shared_mem);  // Aktualizujemy licznik klientów
                safe_sem_post();
                i--;  // Adjust index to check next process in position
            }
        }

        pthread_mutex_unlock(&pid_mutex);  // Zwolnienie mutexu po modyfikacji
    //  usleep(100000);  // Opóźnienie przed kolejnym sprawdzeniem zakończonych procesów
    }

    printf("Koniec czyszczenia procesów.\n");
    return NULL;
}


void* create_customer_processes(void* arg) {
    pid_t pid;

    
    // Sprawdzamy, czy mamy wystarczającą liczbę kasjerów
    while (get_active_cashiers(shared_mem) < MIN_CASHIERS) {
        // sleep(1);
    }

    while (get_fire_flag(shared_mem)!=1) {
        if (safe_sem_wait() == -1) {
            perror("Błąd podczas oczekiwania na semafor");
            pthread_exit(NULL);
        }

        pid = fork();

        // printf("FORK RESULT %d, parent pid :%d\n",getpid(),getppid());
        if (pid <0) {
            //   printf("FORK RESULT w errorze %d, parent pid :%d\n",getpid(),getppid());
            perror("fork failed");
            safe_sem_post();  // Zwolnij semafor w przypadku błędu
            // printf("FORK RESULT %d, w error po sem post parent pid :%d\n",getpid(),getppid());
            pthread_exit(NULL);
        } else if (pid == 0) {
            //   printf("FORK RESULT %d, w dziecku parent pid :%d\n",getpid(),getppid());

            // Mutex do ochrony tablicy PID-ów i zmiennej created_processes
           
            //   printf("FORK RESULT %d,przed exec parent pid :%d\n",getpid(),getppid());
            // Proces klienta   
            if (execl("./src/customer", "customer", (char *)NULL) == -1) {
                perror("Failed to execute customer program");
                 pthread_exit(NULL);
            }
            //   printf("FORK RESULT %d,po exec parent pid :%d\n",getpid(),getppid());
           
        } else {


             pthread_mutex_lock(&pid_mutex);  // Blokujemy mutex przed modyfikacją
            pids[created_processes] = pid;  // Zapisywanie PID procesu potomnego
            created_processes++;  // Zwiększamy licznik procesów
            pthread_mutex_unlock(&pid_mutex);  // Zwolnienie mutexu po modyfikacji
            // printf("w else FORK RESULT %d, parent pid :%d\n",getpid(),getppid());


            // printf("FORK RESULT %d,PO WSZSTKIM parent pid :%d\n",getpid(),getppid());

            // if(get_fire_flag(shared_mem)==1){
            //     printf("ZABITY PROCES PO WSZYTSKIM %d\n", getpid());
            //     exit(0);
            // }
            
        }

        //   printf("FORK RESULT %d, parent pid :%d A TU TO NIE WIEM JZ\n",getpid(),getppid());
    //       int min_time = 200000;  // 0.5 sekundy w mikrosekundach
    // int max_time = 800000; // 1 sekunda w mikrosekundach
    // int random_time = min_time + rand() % (max_time - min_time + 1);
    // usleep(random_time);
    }

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

