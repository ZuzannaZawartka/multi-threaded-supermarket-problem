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

extern int queue_ids[];  // Tablica ID kolejki komunikatów

// Tablica pidow klientow
pid_t customer_pids[MAX_CUSTOMERS];

void* customer_function(void* arg) {
    CustomerData* data = (CustomerData*)arg;  // Odczytanie danych z przekazanej struktury
    pid_t pid = getpid();
    int cashier_id = data->cashier_id;  // Kasjer, do którego klient wysyła komunikat
    
    Message message;

    // Ustawiamy mtype na PID klienta (klient odbiera tylko dla siebie)
    message.mtype = cashier_id;
    message.customer_pid = pid;

    //Wysłanie komunikatu do odpowiedniej kolejki kasjera
    if (msgsnd(queue_ids[cashier_id - 1], &message, sizeof(message) - sizeof(long), 0) == -1) {
        perror("Błąd wysyłania komunikatu");
        exit(1);
    }

    printf("Klient %d wysłał komunikat do kasy %d. Czeka na obsługe \n", pid, cashier_id);


    // Oczekiwanie na komunikat od kasjera, który ma PID klienta
    if (msgrcv(queue_ids[cashier_id - 1], &message, sizeof(message) - sizeof(long), pid, 0) == -1) {
        perror("Błąd odbierania komunikatu od kasjera");
        exit(1);
    }

    //  pause();  // Czekanie na zakończenie obsługi przez kasjera

    printf("Klient %d opuszcza sklep\n", pid);
    free(data);  // Zwolnienie pamięci po zakończeniu działania wątku
    return NULL;
}


// Funkcja do tworzenia procesów klientów
void create_customer_processes(int num_customers, int num_cashiers) {
    for (int i = 0; i < num_customers; i++) {
        int customer_cashier_id = rand() % num_cashiers + 1;  // Losowy kasjer dla klienta

        pid_t pid = fork();
    
        if (pid == 0) { 
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


// Funkcja do usuwania wszystkich klientów
void terminate_all_customers() {
    ProcessNode* current = process_list;
    while (current != NULL) {
        kill(current->pid, SIGTERM);  // Wysyłanie sygnału zakończenia do klienta
        printf("Klient o PID %d został zakończony.\n", current->pid);
        ProcessNode* temp = current;
        current = current->next;
        free(temp);  // Usuwamy element z listy
    }
    process_list = NULL;  // Ustawiamy wskaźnik na NULL
}