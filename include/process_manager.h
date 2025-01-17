#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <sys/types.h>
#include <semaphore.h>

typedef struct ProcessNode {
    pid_t pid;  // PID procesu
    struct ProcessNode* next;  // Wskaźnik do następnego procesu w liście
} ProcessNode;

extern ProcessNode* process_list;  // Zmienna globalna przechowująca początek listy

// Funkcje do zarządzania procesami
void add_process(pid_t pid);
void remove_process(pid_t pid);
int process_exists(pid_t pid);
void cleanup_processes();
void init_semaphore_process() ;
void cleanup_semaphore_process() ;
sem_t* get_semaphore_process() ;

#endif
