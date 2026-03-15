#pragma once
#include <netinet/in.h> // Per a sockaddr_in
#include <pthread.h>    // Per a pthread_t i mutex
#include <csignal>     // Per a sig_atomic_t
#include "ConnexioClient.h"
#include "Dades.h"

#include "ConnexioClient.h"
#include "Dades.h"

#define MAX_CLIENTS 8

// Forward declaration per a l'estructura auxiliar
class ServerCpp;

struct ThreadArgs {
    ServerCpp* servidor;
    ConnexioClient* client;
};

class ServerCpp {
public:
    // --- Cicle de Vida ---
    ServerCpp();
    ~ServerCpp();

    // --- Control del Servidor ---
    void inicialitzar();
    void runServer();
    void stopServer();

    // --- Registre d'Usuaris ---
    int registrar_usuari(const char* username, const char* password);

private:
    // --- Atributs de Xarxa i Estat ---
    struct sockaddr_in config_servidor;
    int socket_escolta;
    volatile sig_atomic_t running;
    const char* version;

    // --- Gestió de Clients i Concurrència ---
    ConnexioClient clients[MAX_CLIENTS];
    pthread_mutex_t semafor_clients;

    static void* gestio_client(void* arg);
    void finalitzar_connexio_client(ConnexioClient* client);
    int buscarPosicioLliure();
    bool ip_ja_connectada(const char* nova_ip);

    // --- Seguretat i Validació ---
    bool existeix_usuari(const char* username);
    bool validar_usuari(const char* usr, const char* pwd);
    void incrementar_comptador(const char* username);
    unsigned long xifrar_password(const char* password);
    void construir_ruta_real(ConnexioClient* client, const char* nom_fitxer, char* ruta_desti);
    int get_operacions_totals(const char* username);


    // --- Funcionalitats (Comandes del Protocol) ---
    void op_cd(ConnexioClient* client);
    int op_dir(ConnexioClient* client);
    int op_get(ConnexioClient* client);
    int op_rget(ConnexioClient* client);
};