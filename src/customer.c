#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include "customer.h"
#include "cashier.h"

extern int queue_ids[];  // Tablica ID kolejki komunikatów

void* customer_function(void* arg) {
    CustomerData* data = (CustomerData*)arg;  // Odczytanie danych z przekazanej struktury
    pid_t pid = getpid();
    int cashier_id = data->cashier_id;  // Kasjer, do którego klient wysyła komunikat
    Message message;

    message.mtype = cashier_id;
    message.customer_pid = pid; 

    //Wysłanie komunikatu do odpowiedniej kolejki kasjera
    if (msgsnd(queue_ids[cashier_id - 1], &message, sizeof(message) - sizeof(long), 0) == -1) {
        perror("Błąd wysyłania komunikatu");
        exit(1);
    }

    printf("Klient %d wysłał komunikat do kasy %d. Czeka na obsługe \n", pid, cashier_id);


    pause();  // Czekanie na zakończenie obsługi przez kasjera

    printf("Klient %d opuszcza sklep\n", pid);
    free(data);  // Zwolnienie pamięci po zakończeniu działania wątku
    return NULL;
}
