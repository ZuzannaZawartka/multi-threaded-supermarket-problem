#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <pthread.h>
#include "cashier.h"
#include "customer.h"

#define NUM_CASHIERS 3
#define NUM_CUSTOMERS 5

//tworzenia procesów klientów
void create_customer_process(int customer_id, int cashier_id) {
    pid_t pid = fork();
    
    if (pid == 0) { 
        customer_function(&cashier_id);  // Klient działa, przypisany do danego kasjera
        
        exit(0); // Zakończenie procesu klienta
    } else if (pid < 0) {
        perror("Bład tworzenia procesu klienta");
        exit(1);
    }
}

int main() {
    pthread_t cashier_threads[NUM_CASHIERS];
    int cashier_ids[NUM_CASHIERS];

    //tworzenie kasjerów
    init_cashiers(cashier_threads, cashier_ids, NUM_CASHIERS);

    // Tworzenie procesów klientów
    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        int customer_cashier_id = rand() % NUM_CASHIERS + 1;  // Losowy kasjer dla klienta
        create_customer_process(i, customer_cashier_id);  // Tworzenie procesu klienta
    }

    // Czekanie na zakończenie wszystkich kasjerów
    for (int i = 0; i < NUM_CASHIERS; i++) {
        pthread_join(cashier_threads[i], NULL);
    }

    // Usuwanie kolejek komunikatów
    for (int i = 0; i < NUM_CASHIERS; i++) {
        msgctl(queue_ids[i], IPC_RMID, NULL);
    }


    return 0;
}
