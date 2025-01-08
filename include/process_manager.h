#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <sys/types.h>

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

#endif
