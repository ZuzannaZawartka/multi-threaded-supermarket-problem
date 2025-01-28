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

/**
 * @brief Główna funkcja procesu klienta.
 * 
 * @details Ustawia handler sygnału i uruchamia zachowanie klienta w sklepie.
 * 
 * @return Zwraca 0 w przypadku pomyślnego zakończenia.
 */
int main() {
    setup_signal_handler_for_customers();   // Ustawienie obsługi sygnałów
    customer_function();                    // Wywołanie zachowania klienta
    return 0;
}

/**
 * @brief Funkcja opisująca zachowanie klienta w sklepie.
 * 
 * @details Klient wchodzi do sklepu, czeka przez losowy czas, wybiera kasjera
 * z najmniejszą liczbą klientów w kolejce, wysyła komunikat do odpowiedniej
 * kolejki kasjera i oczekuje na obsługę. Po obsłudze opuszcza sklep.
 */
void customer_function() {
    shared_mem = get_shared_memory();       // Połączenie z pamięcią dzieloną

    if(get_fire_flag(shared_mem)==1){       // Sprawdzenie, czy został ogłoszony pożar
        exit(0);
    }

    pid_t pid = getpid();
    int stay_time_in_microseconds = generate_random_time(MIN_STAY_CLIENT_TIME, MAX_STAY_CLIENT_TIME);

    printf("\t\033[32mKlient %d przybył do sklepu\033[0m i będzie czekał przez %d ms. [%d/%d]\n",
           pid, stay_time_in_microseconds, get_customer_count(shared_mem), MAX_CUSTOMERS);

    // Klient czeka w sklepie przez losowy czas
    usleep(stay_time_in_microseconds);

    // Wybór kasjera z najmniejszą liczbą klientów
    int cashier_id = select_cashier_with_fewest_people(shared_mem);

    // Przygotowanie wiadomości do kolejki
    Message message;
    message.mtype = cashier_id;
    message.customer_pid = pid;

    // Pobranie ID kolejki dla wybranego kasjera
    int queue_id = get_queue_id(shared_mem, cashier_id);    

    // Wysłanie wiadomości do kolejki kasjera
    if (msgsnd(queue_id, &message, sizeof(message) - sizeof(long), 0) == -1) {
        perror("Błąd wysyłania komunikatu");
        exit(1);
    }

    printf("\t\tKlient %d ( wysłał komunikat do kasy %d. ) Czeka na obsługę.\n", pid, cashier_id);

    // Oczekiwanie na obsługę przez kasjera
    while (1) {
        if (msgrcv(queue_id, &message, sizeof(message) - sizeof(long), pid, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                // Brak wiadomości, klient czeka
                usleep(10000);
                continue;
            } else {
                perror("Błąd odbierania komunikatu klient od kasjera");
                exit(1);
            }
        }
        break;
    }

    // Klient opuszcza sklep po obsłużeniu
    printf("\t\t\t \033[31mKlient %d opuszcza\033[0m sklep, został obsłużony przez kasjera %d \n", pid, cashier_id);

    exit(0); 
}

/**
 * @brief Konfiguruje handler sygnału dla procesu klienta.
 * 
 * @details Ustawia handler dla sygnału SIGTERM. Handler kończy proces klienta
 * w bezpieczny sposób, jeśli zostanie odebrany sygnał.
 */
void setup_signal_handler_for_customers() {
    struct sigaction sa;
    sa.sa_sigaction = handle_customer_signal;       // Funkcja obsługi sygnału
    sa.sa_flags = SA_SIGINFO | SA_RESTART;          // SA_RESTART umożliwia wznowienie przerwanych wywołań systemowych
    sigemptyset(&sa.sa_mask);                       // Inicjalizacja maski sygnałów

    // Rejestracja handlera dla SIGTERM
    if (sigaction(SIGTERM, &sa, NULL) == -1) { 
        perror("Błąd przy ustawianiu handlera dla SIGTERM");
        exit(1);
    }
}

/**
 * @brief Handler sygnału dla procesu klienta.
 * 
 * @param sig Numer sygnału.
 * @param info Informacje o sygnale (np. PID procesu wysyłającego).
 * @param ucontext Kontekst sygnału (niewykorzystywany tutaj).
 * 
 * @details Handler reaguje na sygnał i kończy działanie procesu klienta.
 */
void handle_customer_signal(int sig, siginfo_t *info, void *ucontext) {
    exit(0); // Zakończenie procesu klienta
}

