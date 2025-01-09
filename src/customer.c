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

extern SharedMemory* shared_mem;  // Dostęp do pamięci dzielonej

// Tablica pidow klientow
pid_t customer_pids[MAX_CUSTOMERS];

void* customer_function(void* arg) {
    CustomerData* data = (CustomerData*)arg;  // Odczytanie danych z przekazanej struktury
    pid_t pid = getpid();
    int cashier_id = data->cashier_id;  // Kasjer, do którego klient wysyła komunikat
    int stay_time = generate_random_time(2,5);

    increment_number_customer(shared_mem);
    printf("Klient %d przybył do sklepu i będzie czekał przez %d sekund.\n", pid, stay_time);

    // Cykliczne sprawdzanie flagi, czy pożar jest aktywny
    time_t start_time = time(NULL);  // Czas rozpoczęcia chodzenia klienta po sklepie
    while (difftime(time(NULL), start_time) < stay_time) {
        
        // Sprawdzamy flagę should_exit w pamięci dzielonej
        if (get_should_exit(shared_mem)) {

            decrement_number_customer(shared_mem);  
            free(data);  // Zwolnienie pamięci po zakończeniu działania wątku
            return NULL;
        }

        // Jeśli flaga nie jest ustawiona, klient dalej chodzi po sklepie
        sleep(1);  // Przerwa, aby dać innym procesom czas na działanie
    }


    Message message;
    message.mtype = cashier_id;
    message.customer_pid = pid;

    //sprawdzamy przed wyslaniem komunikatu czy nie ma pozaru
    if (get_should_exit(shared_mem)) {

        decrement_number_customer(shared_mem);  
        free(data);  
        return NULL;
    }

    int queue_id = get_queue_id(shared_mem, cashier_id - 1);//korzystanie z pamieci dzieloenj zeby dostac kolejke

    //Wysłanie komunikatu do odpowiedniej kolejki kasjera
    if (msgsnd(queue_id, &message, sizeof(message) - sizeof(long), 0) == -1) {
        perror("Błąd wysyłania komunikatu");
        exit(1);
    }


    printf("Klient %d wysłał komunikat do kasy %d. Czeka na obsługe \n", pid, cashier_id);


    while (1) {
        // Cykliczne sprawdzanie flagi should_exit w międzyczasie
        if (get_should_exit(shared_mem)) {
            break;
        }

       if (msgrcv(queue_id, &message, sizeof(message) - sizeof(long), pid, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                sleep(1);
                continue;
            } else {
                perror("Błąd odbierania komunikatu");
                exit(1);
            }
        }

        // Jeśli otrzymaliśmy odpowiedź, klient kończy czekanie
        printf("Klient %d otrzymał odpowiedź od kasjera i opuszcza sklep.\n", pid);
        break;
    }


    decrement_number_customer(shared_mem);  
    free(data);  // Zwolnienie pamięci po zakończeniu działania wątku
    return NULL;
}


// Funkcja do tworzenia procesów klientów
void create_customer_processes(int num_customers, int num_cashiers) {
    for (int i = 0; i < num_customers; i++) {
        int customer_cashier_id = rand() % num_cashiers + 1;  // Losowy kasjer dla klienta

        pid_t pid = fork();
    
        if (pid == 0) { 
            signal(SIGINT, SIG_IGN);//IGNOROWANIE SIGINTA W PROCESACH Potomnych bo inaczej na signak handler dla kazdego mamy rekacje
            CustomerData* data = malloc(sizeof(CustomerData));
            data->cashier_id = customer_cashier_id;  // Przypisanie kasjera dla klienta
            customer_function(data);  // Klient działa, przypisany do danego kasjera
            exit(0); // Zakończenie procesu klienta
        } else if (pid < 0) {
            perror("Błąd tworzenia procesu klienta");
            exit(1);
        }

        // Zapisz PID klienta w process managerze
        add_process(pid);
    }
}

void wait_for_customers(int num_customers) {
    int status;
    for (int i = 0; i < num_customers; i++) {
        pid_t finished_pid = waitpid(-1, &status, 0);  // Czekamy na zakończenie dowolnego procesu klienta
        
        if (finished_pid > 0) {
            printf("Proces klienta %d zakończony.\n", finished_pid);
            // Usuwamy PID klienta z listy
            remove_process(finished_pid);
        }
    }
}


// Funkcja do generowania losowego czasu (w sekundach)
int generate_random_time(int min_time, int max_time) {
    return rand() % (max_time - min_time + 1) + min_time;
}
