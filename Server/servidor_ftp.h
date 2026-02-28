#pragma once
#include <pthread.h>
#include <netinet/in.h>

#define PORT 10001
#define MAX_CLIENTS 8

// Protocol binari: header + dades
typedef struct {
    int operacio;
    int versio;
    int len;
    char user[20];
    char pass[20];
} t_header;

typedef struct {
    int socket;
    pthread_t th;
    int ocupat;
} t_client;

void init_taula_clients(t_client* clients, int size);
int pos_taula_clients(t_client* clients, int size);
void* finalitza(t_client* client);
void gestiona_operacio(int clnt, t_header h);
void* func_client(void* d);