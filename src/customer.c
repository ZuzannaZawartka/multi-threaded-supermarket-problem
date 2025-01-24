#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include "shared_memory.h"
#include "main.h"
#include "customer.h"
#include "cashier.h"
#include "creator_customer.h"
#include "manager_cashiers.h"

#define SEMAPHORE_NAME "/customer_semaphore"

// Definicja zmiennej globalnej
SharedMemory* shared_mem = NULL;

// Funkcja obsługi sygnału dla klienta
void handle_customer_signal(int sig, siginfo_t *info, void *ucontext) {
    exit(0); //koniec procesu 
}


// Funkcja zachowania klienta
void customer_function() {
     printf("jest klient w funkcji %d",getpid());
    shared_mem = get_shared_memory();
    increment_customer_count(shared_mem);

    pid_t pid = getpid();
    int stay_time_in_microseconds = generate_random_time(MIN_STAY_CLIENT_TIME, MAX_STAY_CLIENT_TIME);

    printf("\t\033[32mKlient %d przybył do sklepu\033[0m i będzie czekał przez %d ms. [%d/%d]\n",
           pid, stay_time_in_microseconds, get_customer_count(shared_mem), MAX_CUSTOMERS);

    // time_t start_time = time(NULL);
    // while (difftime(time(NULL), start_time) < stay_time_in_microseconds / 1000000.0) {
    //     // exit(1);
    // }

    int cashier_id = select_cashier_with_fewest_people(shared_mem);

    Message message;
    message.mtype = cashier_id;
    message.customer_pid = pid;

    int queue_id = get_queue_id(shared_mem, cashier_id);

    if (msgsnd(queue_id, &message, sizeof(message) - sizeof(long), 0) == -1) {
        perror("Błąd wysyłania komunikatu");
        exit(1);
    }

    printf("\t\tKlient %d ( wysłał komunikat do kasy %d. ) Czeka na obsługę.\n", pid, cashier_id);

    while (1) {
        if (msgrcv(queue_id, &message, sizeof(message) - sizeof(long), pid, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                // usleep(100000);
                continue;
            } else {
                perror("Błąd odbierania komunikatu klient od kasjera");
                exit(1);
            }
        }
        break;
    }

    printf("\t\t\t \033[31mKlient %d opuszcza\033[0m sklep, został obsłużony przez kasjera %d\n", pid, cashier_id);


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


int main() {
    setup_signal_handler_for_customers();
    customer_function();
    return 0;
}

