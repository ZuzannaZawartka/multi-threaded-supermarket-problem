#ifndef MAIN_H
#define MAIN_H

#define MIN_PEOPLE_FOR_CASHIER 3  // Na każdą grupę 5 klientów przypada jeden kasjer
#define MAX_CASHIERS 10 
#define MIN_CASHIERS 2
#define MAX_CUSTOMERS 100  // Maksymalna liczba klientów

#define MIN_TIME_TO_CLIENT 0.1 //czas co jaki generuja sie klienci
#define MAX_TIME_TO_CLIENT 1

#define MIN_STAY_CLIENT_TIME 0.1 //jak długo klient jest w sklepie
#define MAX_STAY_CLIENT_TIME 0.2

#define MIN_CASHIER_OPERATION 40 //czas obslugi kasjera
#define MAX_CASHIER_OPERATION 40

void send_signal_to_manager(int signal);
void send_signal_to_customers(int signal);
void send_signal_to_firefighter(int signal);
void mainHandlerProcess(int signum) ;
void set_process_group();
#endif 
