#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <semaphore.h>
#include <fcntl.h>  // Dla O_CREAT, O_EXCL
#include <sys/stat.h>  // Dla S_IRUSR, S_IWUSR
#include "customer.h"
#include "cashier.h"
#include "shared_memory.h"
#include "manager_cashiers.h"
#include "main.h"

#define SEMAPHORE_NAME "/customer_semaphore"

extern SharedMemory* shared_mem;  // Dostęp do pamięci dzielonej
sem_t* customer_semaphore; 
volatile int terminate_customers = 0;
volatile sig_atomic_t customer_exit_flag = 0; 


// Funkcja pomocnicza do logowania z czasem
void log_semaphore_wait(pid_t pid, const char *semaphore_name, const char *status) {
    time_t now;
    time(&now);
    printf("[%s] PID: %d, Semafor: %s, Status: %s\n", ctime(&now), pid, semaphore_name, status);
}


void setup_signal_handler_for_customers() {
    struct sigaction sa;
    sa.sa_sigaction = handle_customer_signal;  // Obsługuje sygnał dla klienta
    sa.sa_flags = SA_SIGINFO | SA_RESTART;  // Dodaj SA_RESTART, aby wznowić przerwane wywołania systemowe
    sigemptyset(&sa.sa_mask);  // inicjacja maski sygnałów

    if (sigaction(SIGINT, &sa, NULL) == -1) {  // Obsługuje sygnał SIGINT
        perror("Błąd przy ustawianiu handlera dla SIGINT");
        exit(1);
    }
}





void* customer_function() {
    customer_semaphore = get_semaphore_customer();
    increment_customer_count(shared_mem);
    setup_signal_handler_for_customers();//inicjalizacja sigaction żeby działało z semaforem
    pid_t pid = getpid();
    int stay_time_in_microseconds = generate_random_time(MIN_STAY_CLIENT_TIME,MAX_STAY_CLIENT_TIME); //losowanie czasu pobytu klienta
 
    printf("\t\033[32mKlient %d przybył do sklepu\033[0m i będzie czekał przez %d milisekund. [%d/%d] \n", pid, stay_time_in_microseconds,get_customer_count(shared_mem),MAX_CUSTOMERS);

    time_t start_time = time(NULL);  // Czas rozpoczęcia chodzenia klienta po sklepie
    while (difftime(time(NULL), start_time) < stay_time_in_microseconds / 1000000.0) {
        if (customer_exit_flag) {
            goto cleanup;  // Przejście do końca funkcji
        }
        // usleep(100000); // Opóźnienie na 0.1 sekundy, (symulacja klienta w sklepie)
    }

    int cashier_id = 2;//select_cashier_with_fewest_people(shared_mem);//wybór kasjera o najmniejszej kolejce

    Message message;
    message.mtype = cashier_id; //ustawiamy mtype na id kasjera aby kasjer mógł odebrać
    message.customer_pid = pid; //ustawiamy pid klienta

    int queue_id = get_queue_id(shared_mem, cashier_id); // Korzystanie z pamięci dzielonej, żeby dostać kolejkę
    
    // Wysłanie komunikatu do odpowiedniej kolejki kasjera
    if (customer_exit_flag) {
            goto cleanup;  // Przejście do końca funkcji
    }

    if (msgsnd(queue_id, &message, sizeof(message) - sizeof(long), 0) == -1) {
        perror("Błąd wysyłania komunikatu");
        goto cleanup;
    }

    printf("\t\tKlient %d ( wysłał komunikat do kasy %d. ) Czeka na obsługę.\n", pid, cashier_id);

    while (1) {
        if (msgrcv(queue_id, &message, sizeof(message) - sizeof(long), pid, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                if (customer_exit_flag) {
                    goto cleanup;  // Przejście do końca funkcji
                }
                // usleep(100000); //oczekiwanie na wiadomość zwrotną o obsłużeniu przez kasjera
                continue;
            } else {
                perror("Błąd odbierania komunikatu klient od kasjera");
                goto cleanup;
            }
        }
        break;
    }

 
    printf("\t\t\t \033[31mKlient %d opuszcza\033[0m sklep, został obsłużony przez kasjer %d  , obecnie : [%d/%d]\n", pid, cashier_id,get_customer_count(shared_mem)-1,MAX_CUSTOMERS);
    
    cleanup:

    return NULL;
}

//odbiór sygnału o pożarze
void handle_customer_signal(int sig, siginfo_t *info, void *ucontext) {
    // decrement_customer_count(shared_mem);
    // // fflush(stdout);
    //    // Pętla oczekująca na semafor (sem_post)
   
    // printf("Klient %d: Opuszczam sklep.\n", getpid());
    // exit(0);
    customer_exit_flag = 1;
    printf("USTAWIONA FLAGA:()]\n");
}

//odbiór sygnału o końcu tworzenia się nowych klientów
void handle_customer_creating_signal_fire(int sig) {
    terminate_customers =1; //flaga do funkcji wait_for_customer tak że wiemy że to koniec
    printf("JUZ SKONCZYLEM PO SYGNALE\n");
    fflush(stdin);
    pthread_exit(NULL); 
}

//osobny watek na tworzenie klientow
void* create_customer_processes(void* arg) {
    srand(time(NULL));

    if(signal(SIGUSR2, handle_customer_creating_signal_fire) == SIG_ERR) {
        perror("Błąd przy ustawianiu handlera sygnału");
        exit(1);
    }


    printf("aktywni kasjerzy %d",get_active_cashiers(shared_mem) );
    while(get_active_cashiers(shared_mem)<MIN_CASHIERS){ //czekamy na stworzenie się kasjerów
        // usleep(100000);
    }

    while (1) { // Generowanie klientów w nieskończoność
 
        if (safe_sem_wait() == -1) { 
             printf("safe_sem_wait zakończony z błędem.\n");
            continue;
        }

         // Sprawdzamy, czy ustawiono flagę zakończenia
        if (terminate_customers) {
            printf("Tworzenie klientów przerwane przez sygnał (flaga zakończenia).\n");
            safe_sem_post(); // Zwalniamy semafor, bo już nie tworzymy klienta
            pthread_exit(NULL); // Bezpieczne zakończenie wątku
        }


        pid_t pid = fork();  // Tworzenie nowego procesu klienta

        
        if (pid == 0) {
         
            customer_function();// Wywołanie funkcji, która obsługuje zachowanie klienta  
            decrement_customer_count(shared_mem);  // Dekrementacja liczby klientów
                // sem_post(customer_semaphore);
            // if (safe_sem_post() == -1) { 
            //     exit(1);  
            // }
            exit(0); //koniec procesu 
        } else if (pid < 0) {
            perror("Błąd tworzenia procesu klienta");
            if (safe_sem_post() == -1) {
                exit(1); 
            }
            exit(1);
        }

        int random_time = generate_random_time(MIN_TIME_TO_CLIENT, MAX_TIME_TO_CLIENT); 
        usleep(random_time);

    }
}

//funkcja czekająca na kończące sie procesy klientów
void wait_for_customers() {
    int status;
    while (1) {
        pid_t finished_pid = waitpid(-1, &status, WNOHANG);  // Sprawdzamy zakończone procesy
        if (finished_pid > 0) {

        } else if (finished_pid == 0) {
                // Finalizacja i wyjście
            // sleep(1);  // Jeśli nie ma zakończonych procesów, po prostu czekamy
        } else {
            if (errno != ECHILD) {
                perror("Błąd czekania na proces w wait_for_customers");
                exit(1);
            }
        }
        // Sprawdzamy, czy lista procesów jest pusta i czy flaga zakończenia pracy jest ustawiona
        if ( terminate_customers == 1) {
            break;  // Kończymy pętlę, ponieważ procesy zakończone i flaga zakończenia pracy jest ustawiona
        }
    }
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

//usuwanie semafora
void destroy_semaphore_customer() {
    sem_t* semaphore = get_semaphore_customer();
    sem_close(semaphore);  // Close the semaphore
    if (sem_unlink(SEMAPHORE_NAME) == -1) {
        perror("Błąd usuwania semafora");
    } else {
        printf("Semafor został poprawnie usunięty customer.\n");
    }
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

int get_customers_in_shop() {
    sem_t* semaphore = get_semaphore_customer();  // Pobieramy semafor bezpośrednio wewnątrz funkcji
    int value;
    // Krytyczna część: odczytanie wartości semafora
    if (sem_getvalue(semaphore, &value) == -1) {
        perror("Błąd podczas odczytu wartości semafora");
        if (safe_sem_post() == -1) {
            exit(1); 
        }
        return -1;
    }

    // Wyliczenie liczby klientów w sklepie
    int customers_in_shop = MAX_CUSTOMERS - value;


    return customers_in_shop;
}

//blokowanie semafora
int safe_sem_wait() {
    //  sem_t* semaphore = get_semaphore_customer(); 
    // log_semaphore_wait(getpid() , SEMAPHORE_NAME, "Czeka na semafor");
    // if (sem_wait(semaphore) == -1) {
    //     perror("Błąd podczas oczekiwania na semafor");
    //     return -1;
    // }

    // log_semaphore_wait(getpid() , SEMAPHORE_NAME, "Uzyskał dostęp do semafora");
    // return 0;

    sem_t* semaphore = get_semaphore_customer();

    // Czekamy na semafor
    while (sem_wait(semaphore) == -1) {
         log_semaphore_wait(getpid() , SEMAPHORE_NAME, "Czeka na semafor");
        if (errno == EINTR) { // Obsługa sygnałów
            if (terminate_customers) {
                return -1; // Wyjście, jeśli ustawiono flagę
            }
            continue; // Spróbuj ponownie, jeśli przerwanie było inne
        }
        perror("Błąd podczas oczekiwania na semafor");
         log_semaphore_wait(getpid() , SEMAPHORE_NAME, "Uzyskał dostęp do semafora");
        return -1;
    }

    // Flaga sprawdzana natychmiast po uzyskaniu semafora
    if (terminate_customers) {
        sem_post(semaphore); // Zwalniamy semafor, bo już nie chcemy go używać
        return -1; // Wyjście z funkcji
    }

        return 0; // Semafor uzyskany poprawnie
    


}

//zwalnianie semafora
int safe_sem_post() {
    sem_t* semaphore = get_semaphore_customer();

    // Log when trying to release the semaphore
    log_semaphore_wait(getpid(), SEMAPHORE_NAME, "Zwalnia semafor");

    // Retry mechanism for interrupted system calls
    while (sem_post(semaphore) == -1) {
          log_semaphore_wait(getpid(), SEMAPHORE_NAME, "PROBOJE ZWOLNIC semafor");
        if (errno == EINTR) {  // If the call was interrupted by a signal, retry
            continue;
        } else {
            perror("Błąd podczas podnoszenia semafora");
            return -1;  // Return error if it's not an interrupt
        }
    }

       log_semaphore_wait(getpid(), SEMAPHORE_NAME, "SEMAFOR ZWOLNIONY");

    return 0;  // Successfully released the semaphore
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
