#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h> 
#include "servidor_ftp.h"

ControlClient llista_clients[MAX_CLIENTS];
pthread_mutex_t semafor_t_clients = PTHREAD_MUTEX_INITIALIZER;
int socket_escolta;
volatile int running = 1;

void handle_sigint(int sig) {
    printf("\n[INFO] Senyal %d rebut. Tancant servidor...\n", sig);
    running = 0;
    close(socket_escolta);
}

int main() {
    struct sockaddr_in config_servidor, dades_client;
    socklen_t mida_dades_client;

    signal(SIGINT, handle_sigint);

    // Nom de funció corregit segons el .h
    inicialitzar_taula_clients(llista_clients, MAX_CLIENTS);

    socket_escolta = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(socket_escolta, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    config_servidor.sin_family = AF_INET;
    config_servidor.sin_port = htons(PORT_SERVEI);
    config_servidor.sin_addr.s_addr = INADDR_ANY;
    memset(config_servidor.sin_zero, 0, 8);

    if (bind(socket_escolta, (struct sockaddr*)&config_servidor, sizeof(config_servidor)) < 0) {
        perror("Error al BIND");
        return 1;
    }

    listen(socket_escolta, 10);
    printf("=== SERVIDOR ACTIU AL PORT %d (Ctrl+C per sortir) ===\n", PORT_SERVEI);

    while (running) {
        mida_dades_client = sizeof(dades_client);
        int socket_client_nou = accept(socket_escolta, (struct sockaddr*)&dades_client, &mida_dades_client);

        if (socket_client_nou < 0) {
            if (running) perror("Error a l'accept");
            break;
        }

        pthread_mutex_lock(&semafor_t_clients);
        int posicio = buscar_posicio_lliure(llista_clients, MAX_CLIENTS);

        if (posicio >= 0) {
            llista_clients[posicio].esta_ocupat = 1;
            llista_clients[posicio].socket_cli = socket_client_nou;

            pthread_create(&llista_clients[posicio].fil_id, NULL, fil_gestio_client, &llista_clients[posicio]);
            pthread_detach(llista_clients[posicio].fil_id);
        }
        else {
            printf("[ALERTA] Servidor ple. Rebutjant connexió.\n");
            close(socket_client_nou);
        }
        pthread_mutex_unlock(&semafor_t_clients);
    }

    printf("[FI] Servidor aturat correctament.\n");
    return 0;
}