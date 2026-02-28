#include "servidor_ftp.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

// Fem referència al semàfor global definit al main.cpp
extern pthread_mutex_t semafor_taula_clients;

void inicialitzar_taula_clients(ControlClient* llista, int mida) {
    for (int i = 0; i < mida; i++) {
        llista[i].esta_ocupat = 0;
    }
}

int buscar_posicio_lliure(ControlClient* llista, int mida) {
    for (int i = 0; i < mida; i++) {
        if (llista[i].esta_ocupat == 0) return i;
    }
    return -1;
}

void* finalitzar_connexio_client(ControlClient* client_ptr) {
    printf("[INFO] Tancant socket %d\n", client_ptr->socket_comunicacio);
    close(client_ptr->socket_comunicacio);

    pthread_mutex_lock(&semafor_taula_clients);
    client_ptr->esta_ocupat = 0;
    pthread_mutex_unlock(&semafor_taula_clients);

    return NULL;
}

void processar_operacio_matematica(int socket_client, MissatgeHeader header) {
    int numero_a, numero_b, resultat;

    if (header.codi_operacio == 1) { // Suma
        read(socket_client, &numero_a, sizeof(int));
        read(socket_client, &numero_b, sizeof(int));

        resultat = numero_a + numero_b;

        write(socket_client, &resultat, sizeof(int));
        printf("[LOG] Suma feta: %d + %d = %d\n", numero_a, numero_b, resultat);
    }
}

void* fil_gestio_client(void* argument_client) {
    ControlClient* client = (ControlClient*)argument_client;
    MissatgeHeader header_rebut;
    int resposta_validacio = 1;

    if (read(client->socket_comunicacio, &header_rebut, sizeof(MissatgeHeader)) > 0) {
        if (header_rebut.codi_operacio >= 1 && header_rebut.codi_operacio <= 3) {
            resposta_validacio = 0;
        }
        write(client->socket_comunicacio, &resposta_validacio, sizeof(int));

        if (resposta_validacio == 0) {
            processar_operacio_matematica(client->socket_comunicacio, header_rebut);
        }
    }
    return finalitzar_connexio_client(client);
}