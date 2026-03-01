#pragma once
#include <pthread.h>
#include <netinet/in.h>

#define PORT_SERVEI 10235
#define MAX_CLIENTS 8
#define MAX_BUFFER 1024 

// ==================Estructures de dades===========================
//=====================================================================

// Estructura del protocold
typedef struct {
    int operacio;
    int versio;
    char usuari[20];
    char contrasenya[20];
    int len;
} MissatgeHeader;

// Estructura de control del client

typedef struct {
    int socket_cli;
    pthread_t fil_id;
    int esta_ocupat;
    char path_actual[1024];
} ControlClient;

// Definició de codis d'operació
enum CodisOperacio {
    OP_LS = 1,
    OP_CD = 2,
    OP_DOWNLOAD = 3
};


// ==================Funcions de gestió del servidor==================
//====================================================================


void inicialitzar_taula_clients(ControlClient* llista, int mida);
int buscar_posicio_lliure(ControlClient* llista, int mida);
void* finalitzar_connexio_client(ControlClient* client_ptr);
int validar_usuari(char* usr, char* pwd);

/// <summary>
/// Funció que gestiona cada client i crida els mètodes necessaris segons el operacio proporcionat.
/// </summary>
/// <param name="argument_client"></param>
/// <returns></returns>
void* fil_gestio_client(void* argument_client);
void xifrar_password(char* password);


//==================Funcions de funcionalitats del servidor==================
//===========================================================================
void dir_servidor(ControlClient *client);
void cd_path(ControlClient* client);
void download_file(ControlClient* client);
void rget_directory(ControlClient* client);
