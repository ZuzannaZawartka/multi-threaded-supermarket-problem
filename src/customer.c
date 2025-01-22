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
sem_t customer_semaphore; 
volatile int terminate_customers = 0;

extern pthread_mutex_t customers_mutex;
extern pthread_cond_t customers_cond;

void setup_signal_handler_for_customers() {
    struct sigaction sa;
    sa.sa_sigaction = handle_customer_signal;  // Obsługuje sygnał dla klienta
    sa.sa_flags = SA_SIGINFO;  //  flaga SA_SIGINFO, aby otrzymać więcej informacji o sygnale
    sigemptyset(&sa.sa_mask);  // inicjacja maski sygnałów
    if (sigaction(SIGINT, &sa, NULL) == -1) {  // Obsługuje sygnał SIGINT
        perror("Błąd przy ustawianiu handlera dla SIGINT");
        exit(1);
    }
}

void* customer_function() {

    setup_signal_handler_for_customers();//inicjalizacja sigaction żeby działało z semaforem
    pid_t pid = getpid();
    int stay_time_in_microseconds = generate_random_time(MIN_STAY_CLIENT_TIME,MAX_STAY_CLIENT_TIME); //losowanie czasu pobytu klienta
 
    printf("\t\033[32mKlient %d przybył do sklepu\033[0m i będzie czekał przez %d milisekund. [%d/%d] \n", pid, stay_time_in_microseconds,get_customers_in_shop(),MAX_CUSTOMERS);

    time_t start_time = time(NULL);  // Czas rozpoczęcia chodzenia klienta po sklepie
    while (difftime(time(NULL), start_time) < stay_time_in_microseconds / 1000000.0) {
        //  usleep(100000); // Opóźnienie na 0.1 sekundy, (symulacja klienta w sklepie)
    }

    int cashier_id = 2;//rand() % get_active_cashiers(shared_mem); //;;select_cashier_with_fewest_people(shared_mem);//wybór kasjera o najmniejszej kolejce

    Message message;
    message.mtype = cashier_id; //ustawiamy mtype na id kasjera aby kasjer mógł odebrać
    message.customer_pid = pid; //ustawiamy pid klienta

    int queue_id = get_queue_id(shared_mem, cashier_id); // Korzystanie z pamięci dzielonej, żeby dostać kolejkę
    
    // Wysłanie komunikatu do odpowiedniej kolejki kasjera
    if (msgsnd(queue_id, &message, sizeof(message) - sizeof(long), 0) == -1) {
        perror("Błąd wysyłania komunikatu");
        exit(1);
    }

    printf("\t\tKlient %d ( wysłał komunikat do kasy %d. ) Czeka na obsługę.\n", pid, cashier_id);

    while (1) {
        if (msgrcv(queue_id, &message, sizeof(message) - sizeof(long), pid, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                // sleep(1); //oczekiwanie na wiadomość zwrotną o obsłużeniu przez kasjera
                continue;
            } else {
                perror("Błąd odbierania komunikatu klient od kasjera");
                exit(1);
            }
        }
        break;
    }


    printf("\t\t\t \033[31mKlient %d opuszcza\033[0m sklep, został obsłużony przez kasjer %d  , obecnie : [%d/%d]\n", pid, cashier_id,get_customers_in_shop(),MAX_CUSTOMERS);
    
    decrement_customer_count(shared_mem);  // Update shared memory count


    if (safe_sem_post() == -1) {  // Zabezpieczona funkcja sem_post
        exit(1);  // W przypadku błędu, kończymy proces
    }

    return NULL;

}

//odbiór sygnału o pożarze
void handle_customer_signal(int sig, siginfo_t *info, void *ucontext) {
    
    printf("Klient %d: Opuszczam sklep.\n", getpid());
    exit(0);
}

//odbiór sygnału o końcu tworzenia się nowych klientów
void handle_customer_creating_signal_fire(int sig) {
    terminate_customers =1; //flaga do funkcji wait_for_customer tak że wiemy że to koniec
    pthread_exit(NULL); 
}

//osobny watek na tworzenie klientow
void* create_customer_processes(void* arg) {
    srand(time(NULL));

    if(signal(SIGUSR2, handle_customer_creating_signal_fire) == SIG_ERR) {
        perror("Błąd przy ustawianiu handlera sygnału");
        exit(1);
    }


    while(get_active_cashiers(shared_mem)<MIN_CASHIERS){ //czekamy na stworzenie się kasjerów
        //  sleep(1);
    }

    while (1) { // Generowanie klientów w nieskończoność
        if (safe_sem_wait() == -1) { 
            exit(1);  
        }
        pid_t pid = fork();  // Tworzenie nowego procesu klienta

        if (pid == 0) {
            increment_customer_count(shared_mem);
            customer_function();// Wywołanie funkcji, która obsługuje zachowanie klienta
           
            printf("CZY FUNKCJA TU IDZIE?\n");
            exit(0); //koniec procesu 
        } else if (pid < 0) {
            perror("Błąd tworzenia procesu klienta");
            if (safe_sem_post() == -1) {
                exit(1); 
            }
            exit(1);
        }

        int random_time = generate_random_time(MIN_TIME_TO_CLIENT, MAX_TIME_TO_CLIENT); 
        // usleep(random_time);

    }
}

//funkcja czekająca na kończące sie procesy klientów
void wait_for_customers() {
    int status;
    while (1) {
        pid_t finished_pid = waitpid(-1, &status, WNOHANG);  // Sprawdzamy zakończone procesy
        if (finished_pid > 0) {
           
        } else if (finished_pid == 0) {
            //  sleep(1);  // Jeśli nie ma zakończonych procesów, po prostu czekamy
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
        sem_post(semaphore);  // Zwolnienie semafora w przypadku błędu
        return -1;
    }

    // Wyliczenie liczby klientów w sklepie
    int customers_in_shop = MAX_CUSTOMERS - value;


    return customers_in_shop;
}

//blokowanie semafora
int safe_sem_wait() {
    sem_t* semaphore = get_semaphore_customer();
    while (sem_wait(semaphore) == -1) {
        if (errno == EINTR) {
            continue;  // Ponów próbę, jeśli operacja została przerwana sygnałem
        }
        perror("Błąd podczas oczekiwania na semafor");
        return -1;
    }

    return 0;
}

//zwalnianie semafora
int safe_sem_post() {
    sem_t* semaphore = get_semaphore_customer();
    while (sem_post(semaphore) == -1) {
        if (errno == EINTR) {
            continue;  // Ponów próbę, jeśli operacja została przerwana sygnałem
        } else {
            perror("Błąd podczas podnoszenia semafora");
            return -1;  
        }
    }
    decrement_customer_count(shared_mem);
    return 0;  
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
