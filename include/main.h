#ifndef MAIN_H
#define MAIN_H

// Definicje stałych konfiguracyjnych
#define MIN_PEOPLE_FOR_CASHIER 2  // Minimalna liczba klientów przypadających na kasjera
#define MAX_CASHIERS 10           // Maksymalna liczba kasjerów
#define MIN_CASHIERS 2            // Minimalna liczba kasjerów
#define MAX_CUSTOMERS 100         // Maksymalna liczba klientów

#define MIN_TIME_TO_CLIENT 0.1    // Minimalny czas generowania klientów (w sekundach)
#define MAX_TIME_TO_CLIENT 0.2    // Maksymalny czas generowania klientów (w sekundach)

#define MIN_STAY_CLIENT_TIME 0.5  // Minimalny czas przebywania klienta w sklepie (w sekundach)
#define MAX_STAY_CLIENT_TIME 0.7  // Maksymalny czas przebywania klienta w sklepie (w sekundach)

#define MIN_CASHIER_OPERATION 0.5 // Minimalny czas obsługi przez kasjera (w sekundach)
#define MAX_CASHIER_OPERATION 0.7 // Maksymalny czas obsługi przez kasjera (w sekundach)

#define MIN_WAIT_FIREFIGHTER_TIME 15  // Minimalny czas do wywołania strażaka (w sekundach)
#define MAX_WAIT_FIREFIGHTER_TIME 20  // Maksymalny czas do wywołania strażaka (w sekundach)


void send_signal_to_manager(int signal);               // Wysyłanie sygnału do menedżera
void send_signal_to_customers(int signal);             // Wysyłanie sygnału do klientów
void send_signal_to_firefighter(int signal);           // Wysyłanie sygnału do strażaka
void onFireSignalHandler(int signum);                  // Obsługa sygnału pożaru
void onFireSignalHandlerForFirefighter(int signum);    // Obsługa sygnału strażaka
void set_process_group();                              // Ustawienie grupy procesów
void send_signal_to_group(pid_t pgid, int signal);     // Wysyłanie sygnału do grupy procesów
void ignore_signal_for_main(int signum);               // Ignorowanie sygnałów w głównym procesie

#endif
