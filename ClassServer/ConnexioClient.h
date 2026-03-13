#pragma once
#include "Dades.h"
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <string.h>

#define MAX_PATH 512

class ConnexioClient
{
private:
    // ATRIBUTS PRIVATS (Encapsulament)
    char usuari[LEN_USUARI];
    char path_actual[MAX_PATH];
    int socket_cli;
    pthread_t fil_id;
    bool esta_ocupat;
    char ip[INET_ADDRSTRLEN];

public:
    // Constructors
    ConnexioClient();
    ConnexioClient(int socket, const char* ip_client);
    ~ConnexioClient();

    // Mètodes de gestió
    void inicialitzar(int socket, const char* ip_client);

    // Neteja la memòria i tanca el socket de forma segura
    void tancarConnexio();

    // Getters i Setters segurs
    int getSocketCli() const { return socket_cli; }

    pthread_t* getFilIdPtr() { return &fil_id; }

    bool getEstaOcupat() const { return esta_ocupat; }
    void setEstaOcupat(bool ocupat) { esta_ocupat = ocupat; }

    const char* getIpClient() const { return ip; }

    const char* getPathActual() const { return path_actual; }

    void setPathActual(const char* nouPath) {
        strncpy(this->path_actual, nouPath, MAX_PATH - 1);
        this->path_actual[MAX_PATH - 1] = '\0'; 
    }

    const char* getUsuari() const { return usuari; }

    void setUsuari(const char* nomUsuari) {
        strncpy(this->usuari, nomUsuari, sizeof(this->usuari) - 1);
        this->usuari[sizeof(this->usuari) - 1] = '\0';
    }

 
};