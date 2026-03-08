#pragma once
#include <pthread.h>
#include <netinet/in.h>
#include <stdbool.h>
#include "../ClassServer/Dades.h"

#define MAX_CLIENTS 8
#define MAX_BUFFER 1024 

// Estructura de control del client
typedef struct {
    int socket_cli;
    pthread_t fil_id;
    int esta_ocupat;
    char path_actual[1024];
    char ip_client[INET_ADDRSTRLEN];
} ControlClient;



// ==================Funcions de gestió del servidor==================
//====================================================================


void inicialitzar_taula_clients(ControlClient* llista, int mida);
int buscar_posicio_lliure(ControlClient* llista, int mida);
void* finalitzar_connexio_client(ControlClient* client_ptr);
int validar_usuari(char* usr, char* pwd);
void* fil_gestio_client(void* argument_client);
unsigned long xifrar_password(char* password);
int ip_ja_connectada(ControlClient* llista, int mida, char* nova_ip);



//==================Funcions de funcionalitats del servidor==================
//===========================================================================
void dir_servidor(ControlClient *client);
void cd_path(ControlClient* client);
void download_file(ControlClient* client);
void rget_directory(ControlClient* client);
bool existeix_usuari(char* username);
void registrar_usuari(char* username, char* password);
