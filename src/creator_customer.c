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

#define SEMAPHORE_NAME "/customer_semaphore"

extern SharedMemory* shared_mem;  // Dostęp do pamięci dzielonej


// Zmienna atomowa dla flagi
int terminate_customers = 0;

void handle_customer_creating_signal_fire(int sig) {
    terminate_customers=1;
    printf("Zatrzymano tworzenie nowych klientów po sygnale\n");
    pthread_exit(NULL);
}

void* create_customer_processes(void* arg) {
    sem_t* customer_semaphore = get_semaphore_customer();
    srand(time(NULL));

    // Ustawienie sygnału SIGUSR2
    if (signal(SIGUSR2, handle_customer_creating_signal_fire) == SIG_ERR) {
        perror("Błąd przy ustawianiu handlera sygnału");
        pthread_exit(NULL);
    }

    // Czekamy na aktywnych kasjerów
    while (get_active_cashiers(shared_mem) < MIN_CASHIERS) {
        // usleep(100000); // Czekamy na utworzenie kasjerów
    }

    while (!terminate_customers) {
        if (safe_sem_wait() == -1) {
             pthread_exit(NULL);
        }

        pid_t pid = fork();
        printf("Fork result: %d, Current PID: %d, Parent PID: %d\n", pid, getpid(), getppid());

        if (pid == 0) {
            // Proces dziecka
            printf("Child process details:\n");
            printf("  Child PID: %d\n", getpid());
            printf("  Parent PID: %d\n", getppid());

            char current_dir[1024];
            if (getcwd(current_dir, sizeof(current_dir)) != NULL) {
                printf("  Current working directory: %s\n", current_dir);
            }

            // Sprawdzenie flagi przed kontynuowaniem
            // Sprawdzanie flagi w procesie dziecka
            if (terminate_customers) {
                printf("Exiting child process due to termination flag\n");
                exit(0);
            }

            // Uruchomienie procesu dziecka
            char* args[] = {"./src/customer", NULL};
            execvp(args[0], args);  // Uruchamiamy program klienta

            // Jeśli execvp nie powiedzie się, kończymy
            perror("Failed to execute customer program");
            printf("Errno: %d\n", errno);
            exit(1);
        } else if (pid < 0) {
            // Błąd podczas forka
            perror("Fork failed");
            safe_sem_post();
        }

        // Sprawdzamy, czy sygnał zakończenia był aktywowany
        if (terminate_customers) {
            break; // Zakończymy tworzenie nowych klientów
        }

        // Losowy czas oczekiwania przed utworzeniem kolejnego klienta
        int random_time = generate_random_time(MIN_TIME_TO_CLIENT, MAX_TIME_TO_CLIENT);
        usleep(random_time);
    }

    // Kończenie wątku po ustawieniu flagi
    pthread_exit(NULL);
}


// // Obsługa sygnału zakończenia tworzenia klientów
// void handle_customer_creating_signal_fire(int sig) {
//     terminate_customers = 1;
//     printf("Zatrzymano tworzenie nowych klientów po sygnale\n");
//     pthread_exit(NULL);
// }

// void* create_customer_processes(void* arg) {
//     signal(SIGUSR2, handle_customer_creating_signal_fire);

//     while (!terminate_customers) {
//         if (safe_sem_wait() == -1) {
//             continue;
//         }

//         pid_t pid = fork();
        
//         // Detailed debugging
//         printf("Fork result: %d, Current PID: %d, Parent PID: %d\n", 
//                pid, getpid(), getppid());

//         if (pid == 0) {
//             // Child process
//             printf("Child process details:\n");
//             printf("  Child PID: %d\n", getpid());
//             printf("  Parent PID: %d\n", getppid());
//             printf("  Executable path: %s\n", "./src/customer");

//             // Add absolute path debugging
//             char current_dir[1024];
//             if (getcwd(current_dir, sizeof(current_dir)) != NULL) {
//                 printf("  Current working directory: %s\n", current_dir);
//             }

//             if (terminate_customers) {
//                 printf("Exiting child process due to termination flag\n");
//                 exit(0);
//             }

//             char* args[] = {"./src/customer", NULL};
//             execvp(args[0], args);

//             // Extensive error reporting
//             perror("Failed to execute customer program");
//             printf("Errno: %d\n", errno);
//             exit(1);
//         } else if (pid < 0) {
//             perror("Fork failed");
//             safe_sem_post();
//         }
//     }

//     pthread_exit(NULL);
// }

void wait_for_customers() {

    sem_t* customer_semaphore = get_semaphore_customer();

    printf("Czekam na zakończenie wszystkich klientów...\n");

    while (1) {
        
        pid_t finished_pid = wait(NULL); // Czekamy na dowolne dziecko
        if (finished_pid > 0) {
            decrement_customer_count(shared_mem);  // Aktualizujemy licznik klientów
            sem_post(customer_semaphore);
            printf("Proces klienta %d zakończony. Pozostało klientów: %d\n", 
                   finished_pid, get_customer_count(shared_mem));
        } else {
            if (errno == ECHILD) {
                if (terminate_customers == 1 && get_customer_count(shared_mem) <= 0) {
                        break;
                }
                continue;
            } else {
                perror("Błąd czekania na proces w wait_for_customers");
                exit(1);
            }
        }

        // Jeśli flaga zakończenia pracy jest ustawiona i wszyscy klienci zakończyli
        if (terminate_customers == 1 && get_customer_count(shared_mem) <= 0) {
            break;
        }
    }

    printf("KONIEC WAIT FOR CUSTOMER\n");
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

    // Logowanie prób czekania na semafor
    log_semaphore_wait(getpid(), SEMAPHORE_NAME, "Czeka na semafor");

    while (sem_wait(semaphore) == -1) {
        if (errno == EINTR) {  // Obsługa przerwania przez sygnał
            // Logowanie, gdy oczekiwanie na semafor jest przerwane
            log_semaphore_wait(getpid(), SEMAPHORE_NAME, "Przerwano czekanie na semafor przez sygnał");

            if (terminate_customers) {
                log_semaphore_wait(getpid(), SEMAPHORE_NAME, "Flaga zakończenia ustawiona, kończymy czekanie na semafor");
                return -1;
            }
            continue;  // Jeśli przerwanie było inne, próbuj ponownie
        }

        // Logowanie błędu w przypadku innego problemu z semaforem
        perror("Błąd podczas oczekiwania na semafor");
        log_semaphore_wait(getpid(), SEMAPHORE_NAME, "Błąd podczas oczekiwania na semafor");
        return -1;
    }

    // Flaga sprawdzana po uzyskaniu semafora
    if (terminate_customers) {
        sem_post(semaphore); // Zwolnienie semafora, bo już go nie potrzebujemy
        log_semaphore_wait(getpid(), SEMAPHORE_NAME, "Zwolniono semafor przed zakończeniem");
        return -1;
    }

    // Logowanie uzyskania dostępu do semafora
    log_semaphore_wait(getpid(), SEMAPHORE_NAME, "Uzyskano dostęp do semafora");

    return 0;  // Semafor uzyskany poprawnie
}

int safe_sem_post() {
    sem_t* semaphore = get_semaphore_customer();

    // Logowanie próby zwolnienia semafora
    log_semaphore_wait(getpid(), SEMAPHORE_NAME, "Zwalnianie semafora");

    while (sem_post(semaphore) == -1) {
        if (errno == EINTR) {  // Przerwanie przez sygnał
            // Logowanie próby ponownego zwolnienia semafora
            log_semaphore_wait(getpid(), SEMAPHORE_NAME, "Przerwano zwalnianie semafora przez sygnał");
            continue;  // Spróbuj ponownie
        } else {
            // Logowanie błędu, jeśli semafor nie może zostać zwolniony
            perror("Błąd podczas zwalniania semafora");
            log_semaphore_wait(getpid(), SEMAPHORE_NAME, "Błąd podczas zwalniania semafora");
            return -1;
        }
    }

    // Logowanie zwolnienia semafora
    log_semaphore_wait(getpid(), SEMAPHORE_NAME, "Semafor zwolniony");

    return 0;  // Semafor zwolniony pomyślnie
}



//czas w mikrosekundach
int generate_random_time(float min_seconds, float max_seconds) {
    if (min_seconds < 0 || max_seconds < 0 || min_seconds > max_seconds) {
        fprintf(stderr, "Błędne wartości zakresu czasu. MIN >= 0, MAX >= 0, a MIN <= MAX.\n");
        return -1;
    }

    // Zamiana sekund na mikrosekundy
    int min_microseconds = (int)(min_seconds * 1000000);
    int max_microseconds = (int)(max_seconds * 1000000);

    // Losowy czas w mikrosekundach
    return (rand() % (max_microseconds - min_microseconds + 1)) + min_microseconds;
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