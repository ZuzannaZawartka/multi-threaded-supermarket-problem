#ifndef CREATOR_CUSTOMER_H
#define CREATOR_CUSTOMER_H

#include <sys/types.h>  // For pid_t
#include <semaphore.h>  // For sem_t
#include <fcntl.h>      // For O_CREAT, O_EXCL
#include <sys/stat.h>   // For mode constants
#include <signal.h>     // For signal handling

// Structure to hold customer data
typedef struct {
    int cashier_id;  // Cashier's ID
    pid_t pid;       // PID of the customer process
} CustomerData;


void* create_customer_processes(void* arg);          // Creates customer processes
void wait_for_customers();                           // Wait for all customer processes to finish
void terminate_all_customers();                      // Terminate all customer processes
int get_customers_in_shop();                         // Returns the number of customers in the shop

void init_semaphore_customer();                      // Initialize customer semaphore
void destroy_semaphore_customer();                   // Destroy customer semaphore
// sem_t* get_semaphore_customer();                     // Retrieve the customer semaphore
sem_t* get_semaphore_customer();
int safe_sem_wait();                                 // Safe wait on a semaphore
int safe_sem_post();                                 // Safe post to a semaphore

int generate_random_time(float min_seconds, float max_seconds);  // Generate random time in microseconds
void handle_customer_creating_signal_fire(int sig);  // Handle signal to stop creating customers

void log_semaphore_wait(pid_t pid, const char* semaphore_name, const char* message);  // Log semaphore actions


#endif // CREATOR_CUSTOMER_H
