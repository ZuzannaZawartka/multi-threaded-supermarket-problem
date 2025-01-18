#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include "customer.h"
#include "cashier.h"
#include "process_manager.h"
#include <sys/wait.h>
#include <errno.h>
#include "shared_memory.h"
#include "manager_cashiers.h"


#include <semaphore.h>
#include <fcntl.h>  // Dla O_CREAT, O_EXCL
#include <sys/stat.h>  // Dla S_IRUSR, S_IWUSR

#define MIN_CASHIERS 2

#define SEMAPHORE_NAME "/customer_semaphore"


extern SharedMemory* shared_mem;  // Dostęp do pamięci dzielonej
sem_t customer_semaphore; 
volatile int terminate_customers = 0;


void setup_signal_handler_for_customers() {
    struct sigaction sa;
    sa.sa_sigaction = handle_customer_signal;  // Obsługuje sygnał dla klienta
    sa.sa_flags = SA_SIGINFO;  // Użyj flagi SA_SIGINFO, aby otrzymać więcej informacji o sygnale
    sigemptyset(&sa.sa_mask);  // Zainicjuj maskę sygnałów
    if (sigaction(SIGINT, &sa, NULL) == -1) {  // Obsługuje sygnał SIGINT
        perror("Błąd przy ustawianiu handlera dla SIGINT");
        exit(1);
    }

}

// Funkcja generująca losowy czas od 0.5 do 3 sekund (w mikrosekundach)
int generate_random_time() {
    return (rand() % (3000000 - 500000 + 1)) + 500000; // Mikrosekundy
}

void* customer_function() {

    setup_signal_handler_for_customers();

    // signal(SIGINT, handle_customer_signal); // handler na sygnał i wyjściu

    pid_t pid = getpid();
    int stay_time_in_microseconds = generate_random_time();

    printf("\t\033[32mKlient %d przybył do sklepu\033[0m i będzie czekał przez %d milisekund. [%d/100] \n", pid, stay_time_in_microseconds, get_customers_in_shop());

    time_t start_time = time(NULL);  // Czas rozpoczęcia chodzenia klienta po sklepie
    while (difftime(time(NULL), start_time) < stay_time_in_microseconds / 1000000.0) {
        // Klient dalej chodzi po sklepie
        usleep(100000); // Opóźnienie na 0.1 sekundy, by dać czas innym procesom
    }

    int cashier_id = select_cashier_with_fewest_people(shared_mem);

    Message message;
    message.mtype = cashier_id;
    message.customer_pid = pid;

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
                sleep(1);
                continue;
            } else {
                printf("Blad odbierania komunikatu od kasjera %d",cashier_id);
                perror("Błąd odbierania komunikatu klient od kasjera");
                exit(1);
            }
        }
        printf("\t\t\t \033[31mKlient %d opuszcza\033[0m sklep, został obsłużony przez kasjer %d \n", pid, cashier_id);
        break;
    }
    
    return NULL;
}


void handle_customer_signal(int sig, siginfo_t *info, void *ucontext) {
    // Klient czeka przez 5 sekund przed wyjściem
    sleep(5);

    if (safe_sem_post() == -1) {  // Zabezpieczona funkcja sem_post
            exit(1);  // W przypadku błędu, kończymy proces
    }

    printf("Klient %d: Opuszczam sklep.\n", getpid());
    exit(0);
}


//wylaczenie tworzenia nowych customerow
void handle_customer_signal2(int sig) {
    terminate_customers =1;
    pthread_exit(NULL); 
}


//osobny watek na tworzenie klientow
void* create_customer_processes(void* arg) {
    srand(time(NULL));

    signal(SIGUSR2, handle_customer_signal2);

    init_semaphore_customer();

    while(get_active_cashiers(shared_mem)<MIN_CASHIERS){ //czekamy na stworzenie sie kasjerow
        sleep(1);
    }

    while (1) {  // Generowanie klientów w nieskończoność
        if (safe_sem_wait() == -1) {  // Zabezpieczona funkcja sem_wait
            exit(1);  // W przypadku błędu, kończymy proces
        }

        pid_t pid = fork();  // Tworzenie nowego procesu klienta

        if (pid == 0) {
            customer_function();// Wywołanie funkcji, która obsługuje zachowanie klienta

            if (safe_sem_post() == -1) {  // Zabezpieczona funkcja sem_post
                exit(1);  // W przypadku błędu, kończymy proces
            }

            exit(0);
        } else if (pid < 0) {
            perror("Błąd tworzenia procesu klienta");
            
            if (safe_sem_post() == -1) {  // Zabezpieczona funkcja sem_post
                exit(1);  // W przypadku błędu, kończymy proces
            }
            exit(1);
        }

        // Zapisz PID klienta w process managerze
        add_process(pid);


        int random_time_in_microseconds = (rand() % (3000000 - 500000 + 1)) + 500000;
        usleep(random_time_in_microseconds);

    }

    return NULL;  // Funkcja musi zwracać 'void*'
}

void wait_for_customers() {
    int status;

    // Pętla czekająca na zakończenie wszystkich procesów klientów.
    while (1) {
        // Sprawdzamy zakończone procesy
        pid_t finished_pid = waitpid(-1, &status, WNOHANG);  // Sprawdzamy zakończone procesy
        if (finished_pid > 0) {
            // Jeżeli proces zakończony, usuwamy go z listy
            remove_process(finished_pid);  // Usuwamy PID z listy

            // printf("Proces klienta %d zakończony, zwiększam semafor. Miejsca w sklepie zostały zwolnione.\n", finished_pid);
        } else if (finished_pid == 0) {

            sleep(1);  // Jeśli nie ma zakończonych procesów, po prostu czekamy
        } else {
            // Obsługujemy błąd (np. brak procesów do oczekiwania)
            if (errno != ECHILD) {
                perror("Błąd czekania na proces w wait_for_customers");
                exit(1);
            }
        }

        // Sprawdzamy, czy lista procesów jest pusta i czy flaga zakończenia pracy jest ustawiona
        if (process_list == NULL && terminate_customers == 1) {
            cleanup_processes(); // Po zakończeniu pracy, czyszczymy wszystkie procesy
            break;  // Kończymy pętlę, ponieważ procesy zakończone i flaga zakończenia pracy jest ustawiona
        }
    }
}



void init_semaphore_customer() {
    sem_t* semaphore = sem_open(SEMAPHORE_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 100);
    if (semaphore == SEM_FAILED) {
        if (errno == EEXIST) {
            printf("Semafor już istnieje, usuwam i tworzę nowy.\n");
            sem_unlink(SEMAPHORE_NAME);
            semaphore = sem_open(SEMAPHORE_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 100);
        }
        if (semaphore == SEM_FAILED) {
            perror("Błąd inicjalizacji semafora");
            exit(1);
        }
    }
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



sem_t* get_semaphore_customer() {
    sem_t* semaphore = sem_open(SEMAPHORE_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 100);
    
    if (semaphore == SEM_FAILED) {
        // Sprawdzamy, czy błąd to EEXIST, który oznacza, że semafor już istnieje
        if (errno == EEXIST) {
            // printf("Semafor już istnieje, otwieram go.\n");
            // Semafor już istnieje, więc otwieramy go
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


// Funkcja odczytująca ile osób jest aktualnie w sklepie
int get_customers_in_shop() {
    sem_t* semaphore = get_semaphore_customer();  // Pobieramy semafor bezpośrednio wewnątrz funkcji

    int value;
    if (sem_getvalue(semaphore, &value) == -1) {
        perror("Błąd podczas odczytu wartości semafora");
        return -1;
    }

    // Liczba osób w sklepie to różnica pomiędzy maksymalną wartością semafora a aktualną wartością semafora
    int customers_in_shop = 100 - value;
    // printf("Aktualna liczba osób w sklepie: %d / 100\n", customers_in_shop);
    return customers_in_shop;
}


int safe_sem_wait() {
    sem_t* semaphore = get_semaphore_customer();
    while (sem_wait(semaphore) == -1) {
        if (errno == EINTR) {
            continue;  // Retry if interrupted
        }
        perror("Błąd podczas oczekiwania na semafor");
        return -1;
    }
    return 0;
}

int safe_sem_post() {
    sem_t* semaphore = get_semaphore_customer();
    while (sem_post(semaphore) == -1) {
        if (errno == EINTR) {
            continue;  // Ponów próbę, jeśli operacja została przerwana sygnałem
        } else {
            perror("Błąd podczas podnoszenia semafora");
            return -1;  // Zwróć -1 w przypadku innych błędów
        }
    }

    return 0;  // Operacja zakończona sukcesem
}



//ps aux | grep 'Z' | grep zawart |grep supermarket |cut -d ' ' -f 2 | xargs kill -9 -usuwanie zombie
//ps aux |grep zawartk |grep supermarket  -procesy 

//synchronizacaj zeby klienci wchodzi dopiero jak sa kasjerzy