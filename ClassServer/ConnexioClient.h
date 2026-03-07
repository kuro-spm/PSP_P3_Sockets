#pragma once
#include "Dades.h"
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <string.h>

#define PATH_DEFECTE "./" // Path per defecte quan un client es connecta
#define MAX_PATH 256

class ConnexioClient
{
private:
    // ATRIBUTS PRIVATS (Encapsulament)
    std::string usuari;       // std::string gestiona la memòria per tu
    std::string path_actual;

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

    // Retornem const char* per compatibilitat amb funcions de C (com opendir)
    const char* getPathActual() const { return path_actual.c_str(); }    
    void setPathActual(const std::string& nouPath) { path_actual = nouPath; }

    const char* getIpClient() const { return ip; }

    const std::string& getUsuari() const { return usuari; }
	void setUsuari(const std::string& nomUsuari) { usuari = nomUsuari; }

 
};