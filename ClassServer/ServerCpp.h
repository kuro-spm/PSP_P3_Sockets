#pragma once
#include "ConnexioClient.h"
#include "Dades.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <string>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <csignal>

#define MAX_CLIENTS 8


class ServerCpp
{
private:
    struct sockaddr_in config_servidor;
    int socket_escolta;
    ConnexioClient clients[MAX_CLIENTS];
    pthread_mutex_t semafor_clients;
    volatile sig_atomic_t running;
    std::string version;

    // Mètodes auxiliars interns de seguretat i dades
    bool existeix_usuari(const std::string& username);
    bool validar_usuari(const std::string& usr, const std::string& pwd);
    bool ip_ja_connectada(const char* nova_ip);
    unsigned long xifrar_password(const std::string& password);
    int buscarPosicioLliure();


public:
    ServerCpp();
    ~ServerCpp();

    void inicialitzar();
    void runServer();
    void stopServer();

    static void* gestio_client(void* arg);
	void finalitzar_connexio_client(ConnexioClient* client);



    //================== Funcionalitats del servidor ==================
    // Ara reben l'objecte ConnexioClient que té tota la info (socket, path, etc.)
    void registrar_usuari(const std::string& username, const std::string& password);
    void dir_servidor(ConnexioClient* client);
    void cd_path(ConnexioClient* client);
    void download_file(ConnexioClient* client);
    void rget_directory(ConnexioClient* client);

};

// Estructura auxiliar per passar dades al fil
struct ThreadArgs {
    ServerCpp* servidor;
    ConnexioClient* client;
};