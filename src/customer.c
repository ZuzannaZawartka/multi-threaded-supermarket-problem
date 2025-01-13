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


#include <semaphore.h>
#include <fcntl.h>  // Dla O_CREAT, O_EXCL
#include <sys/stat.h>  // Dla S_IRUSR, S_IWUSR


#define SEMAPHORE_NAME "/customer_semaphore" 

extern SharedMemory* shared_mem;  // Dostęp do pamięci dzielonej
sem_t customer_semaphore; 


void* customer_function(void* arg) {

    signal(SIGINT, handle_customer_signal); // handler na sygnał i wyjściu

    CustomerData* data = (CustomerData*)arg;  // Odczytanie danych z przekazanej struktury
    pid_t pid = getpid();
    int cashier_id = data->cashier_id;  // Kasjer, do którego klient wysyła komunikat
    int stay_time = generate_random_time(2,3);

    printf("Klient %d przybył do sklepu i będzie czekał przez %d sekund.\n", pid, stay_time);

    // Cykliczne sprawdzanie flagi, czy pożar jest aktywny
    time_t start_time = time(NULL);  // Czas rozpoczęcia chodzenia klienta po sklepie
    while (difftime(time(NULL), start_time) < stay_time) {
        // Jeśli flaga nie jest ustawiona, klient dalej chodzi po sklepie
        sleep(1);  // Przerwa, aby dać innym procesom czas na działanie
    }

    Message message;
    message.mtype = cashier_id;
    message.customer_pid = pid;

    int queue_id = get_queue_id(shared_mem, cashier_id); // Korzystanie z pamięci dzielonej, żeby dostać kolejkę
    
    // Wysłanie komunikatu do odpowiedniej kolejki kasjera
    if (msgsnd(queue_id, &message, sizeof(message) - sizeof(long), 0) == -1) {
        perror("Błąd wysyłania komunikatu");
        exit(1);
    }

    printf("Klient %d wysłał komunikat do kasy %d. Czeka na obsługę.\n", pid, cashier_id);

    while (1) {
        if (msgrcv(queue_id, &message, sizeof(message) - sizeof(long), pid, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                sleep(1);
                continue;
            } else {
                perror("Błąd odbierania komunikatu klient");
                exit(1);
            }
        }

        // Jeśli otrzymaliśmy odpowiedź, klient kończy czekanie
        printf("Klient %d otrzymał odpowiedź od kasjera i opuszcza sklep.\n", pid);
        break;
    }
    
    // Zwolnij pamięć tylko raz
    free(data);  // Zwolnienie pamięci po zakończeniu działania wątku
    return NULL;
}


void handle_customer_signal(int sig) {
    printf("Klient %d: Odebrałem sygnał pożaru! Rozpoczynam ewakuację...\n", getpid());
    printf("Klient %d: Opuszczam sklep.\n", getpid());
    remove_process(getpid());
    exit(0);
}


//wylaczenie tworzenia nowych customerow
void handle_customer_signal2(int sig) {

    cleanup_processes();  // Oczyszczenie pamięci związanej z procesami
    destroy_semaphore_customer();  // Zwalniamy semafor
    pthread_exit(NULL); 
}


//osobny watek na tworzenie klientow
void* create_customer_processes(void* arg) {
    signal(SIGUSR2, handle_customer_signal2);

    init_semaphore_customer();
    sem_t* semaphore = get_semaphore_customer();  // Pobieramy semafor
   
    while (1) {  // Generowanie klientów w nieskończoność
        if (safe_sem_wait(semaphore) == -1) {  // Zabezpieczona funkcja sem_wait
            exit(1);  // W przypadku błędu, kończymy proces
        }

        // Losowy wybór kasjera dla klienta
        int customer_cashier_id = rand() % 2 + 1; //2 ZAMIENIC NA KASJEROW

        pid_t pid = fork();  // Tworzenie nowego procesu klienta

        if (pid == 0) {
            // Alokacja pamięci dla danych klienta w procesie potomnym
            CustomerData* data = malloc(sizeof(CustomerData));
            if (data == NULL) {
                perror("Błąd alokacji pamięci");
                exit(1);  // Zakończenie programu w przypadku błędu alokacji
            }

            // Ustawienie danych klienta
            data->cashier_id = customer_cashier_id;

            // Wywołanie funkcji, która obsługuje zachowanie klienta
            customer_function(data);  // Klient działa, przypisany do danego kasjera

            if (safe_sem_post(semaphore) == -1) {  // Zabezpieczona funkcja sem_post
                exit(1);  // W przypadku błędu, kończymy proces
            }
            get_customers_in_shop(semaphore); //ilosc osob po wyjsciu ze sklepu

            exit(0);
        } else if (pid < 0) {
            perror("Błąd tworzenia procesu klienta");
            
            if (sem_post(semaphore) == -1) {
                perror("Błąd podczas podnoszenia semafora po zakończeniu klienta");
                exit(1);
            }
            exit(1);
        }

        // Zapisz PID klienta w process managerze
        add_process(pid);

        get_customers_in_shop(semaphore);
        // Losowy czas na następnego klienta
        sleep(generate_random_time(5, 7));  // Klient może przyjść w losowych odstępach czasu
    }

    // Zamykamy semafor po zakończeniu tworzenia procesów
    sem_close(semaphore);

    return NULL;  // Funkcja musi zwracać 'void*'
}


void wait_for_customers() {
   int status;

    // Czekamy na klientów, jeśli lista jest pusta
    while (1) {
        if (process_list != NULL) {
            break;  // Jeśli lista nie jest pusta, wychodzimy z pętli
        }

        printf("Brak procesów klientów. Czekam na nowych klientów...\n");
        sleep(1);  // Krótkie opóźnienie, aby dać czas na dodanie nowych procesów
    }

    // Pętla oczekująca na zakończenie procesów klientów
    while (process_list != NULL) {
        pid_t finished_pid = waitpid(-1, &status, WNOHANG);  // Sprawdzamy, czy jakieś procesy już zakończone
        if (finished_pid > 0) {
            // Jeśli proces zakończony, usuwamy go z listy
            printf("Proces klienta %d zakończony.\n", finished_pid);
            remove_process(finished_pid);  // Usuwamy PID zakończonego procesu z listy
        } else if (finished_pid == 0) {
            // Jeśli brak zakończonych procesów, czekamy i próbujemy ponownie
            //W sklepie nie zawsze są klienci
            sleep(1);
        } else {
            // Obsługujemy błąd, jeśli np. waitpid nie może znaleźć procesu
            if (errno != ECHILD) {
                perror("Błąd czekania na proces");
                exit(1);
            }
        }
    }
}


// Funkcja do generowania losowego czasu (w sekundach)
int generate_random_time(int min_time, int max_time) {
    return rand() % (max_time - min_time + 1) + min_time;
}



//semafor do zliczania maksymalnej ilosci osob w sklepie
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
    printf("Semafor został zainicjalizowany z wartością 100.\n");
    sem_close(semaphore);
}


void destroy_semaphore_customer() {
    if (sem_unlink(SEMAPHORE_NAME) == -1) {
        perror("Błąd usuwania semafora");
    } else {
        printf("Semafor został poprawnie usunięty customer.\n");
    }
}


sem_t* get_semaphore_customer() {
    sem_t* semaphore = sem_open(SEMAPHORE_NAME, 0);  // Otwieramy istniejący semafor
    if (semaphore == SEM_FAILED) {
        perror("Błąd otwierania semafora");
        exit(1);
    }
    return semaphore;
}

// Funkcja odczytująca ile osób jest aktualnie w sklepie
void get_customers_in_shop(sem_t *semaphore) {
    int value;
    if (sem_getvalue(semaphore, &value) == -1) {
        perror("Błąd podczas odczytu wartości semafora");
        return;
    }
    // Liczba osób w sklepie to różnica pomiędzy maksymalną wartością semafora a aktualną wartością semafora
    int customers_in_shop = 100 - value;
    printf("Aktualna liczba osób w sklepie: %d / 100\n", customers_in_shop);
}


int safe_sem_wait(sem_t *semaphore) {
    if (semaphore == NULL) {
        fprintf(stderr, "Błąd: Semafor nie został zainicjowany.\n");
        return -1;  // Zwracamy -1, aby wskazać błąd
    }
    
    int result = sem_wait(semaphore);
    if (result == -1) {
        perror("Błąd podczas oczekiwania na semafor");
        return -1;  // Zwracamy -1 w przypadku błędu
    }

    return 0;  // Zwracamy 0, jeśli operacja zakończyła się sukcesem
}


int safe_sem_post(sem_t *semaphore) {
    if (semaphore == NULL) {
        fprintf(stderr, "Błąd: Semafor nie został zainicjowany.\n");
        return -1;  // Zwracamy -1, aby wskazać błąd
    }

    int result = sem_post(semaphore);
    if (result == -1) {
        perror("Błąd podczas podnoszenia semafora");
        return -1;  // Zwracamy -1 w przypadku błędu
    }

    return 0;  // Zwracamy 0, jeśli operacja zakończyła się sukcesem
}


