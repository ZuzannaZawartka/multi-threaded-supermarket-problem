#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <sys/types.h>
#include <semaphore.h>

void setup_signal_handler_for_customers();  // Ustawia obsługę sygnałów dla klientów
void handle_customer_signal(int sig, siginfo_t *info, void *ucontext);  // Obsługa sygnału klienta
void customer_function();  // Główna funkcja procesu klienta
#endif