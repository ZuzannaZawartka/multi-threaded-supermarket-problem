#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <sys/types.h>
#include <semaphore.h>


void setup_signal_handler_for_customers();
void handle_customer_signal(int sig, siginfo_t *info, void *ucontext);

// Funkcja zachowania klienta
void customer_function();
#endif