#include <sys/types.h> 
#define MAX_CASH_REGISTER 10

typedef struct {
    int register_id;       // Numer kasy
    int clients_in_queue;  // Liczba klientów w kolejce
    int is_open;           // 1 - Kasa otwarta, 0 - Kasa zamknięta
    pid_t pid;  
} Cashier;


void initialize_cashier(Cashier* cashier, int register_id);
void open_cashier(Cashier* cashier);
void close_cashier(Cashier* cashier);
void serve_clients(Cashier* cashier);
void update_queue(Cashier* cashier, int num_clients);
