#ifndef MAIN_H
#define MAIN_H

#define MIN_PEOPLE_FOR_CASHIER 5  // Na każdą grupę 5 klientów przypada jeden kasjer
#define MAX_CASHIERS 10 //maksymalna liczba kasjerów
#define MIN_CASHIERS 2 //minimalna liczba kasjerów
#define MAX_CUSTOMERS 400  // Maksymalna liczba klientów

#define MIN_TIME_TO_CLIENT 0 //czas co jaki generuja sie klienci (Kasjer sprawdza stan klientów co 0.5 sekundy)
#define MAX_TIME_TO_CLIENT 0

#define MIN_STAY_CLIENT_TIME 3 //jak długo klient jest w sklepie
#define MAX_STAY_CLIENT_TIME 5

#define MIN_CASHIER_OPERATION 1//czas obslugi kasjera
#define MAX_CASHIER_OPERATION 2

#define MIN_WAIT_FIREFIGHTER_TIME 120 //po jakim czasie wywola sie strazak
#define MAX_WAIT_FIREFIGHTER_TIME 160

void send_signal_to_manager(int signal);
void send_signal_to_customers(int signal);
void send_signal_to_firefighter(int signal);
void mainHandlerProcess(int signum) ;
void set_process_group();
void send_signal_to_group(pid_t pgid, int signal) ;

void ignore_signal_for_main(int signum);
#endif 
