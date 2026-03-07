#include "ConnexioClient.h"

/// <summary>
/// Constructor per defecte: Inicialitza els atributs a valors segurs i per defecte.
/// socket_cli a -1 (indicant que no hi ha connexió), esta_ocupat a false, path_actual a "." (directori actual) i ip buida.
/// Aquest constructor és útil quan es crea un objecte ConnexioClient sense dades immediates, i es vol inicialitzar més tard amb el mètode inicialitzar().
/// </summary>
ConnexioClient::ConnexioClient() {
    socket_cli = SOCKET_ATURAT;
    esta_ocupat = false;
    path_actual = PATH_DEFECTE; 
    memset(ip, 0, INET_ADDRSTRLEN);
}

// Constructor amb dades
ConnexioClient::ConnexioClient(int socket, const char* ip_client) {
    inicialitzar(socket, ip_client);
}

// Destructor
ConnexioClient::~ConnexioClient() {
    tancarConnexio();
}

/// <summary>
/// Inicializa la conexión del cliente: asigna el descriptor de socket, marca la conexión como ocupada, restablece el path al valor por defecto y copia la dirección IP del cliente si se proporciona.
/// </summary>
/// <param name="socket">Descriptor de socket del cliente (entero).</param>
/// <param name="ip_client">Cadena con la dirección IP del cliente; puede ser NULL. Si no es NULL, se copia en el campo interno de IP usando strncpy hasta INET_ADDRSTRLEN bytes.</param>
void ConnexioClient::inicialitzar(int socket, const char* ip_client) {
    socket_cli = socket;
    esta_ocupat = true;
    path_actual = PATH_DEFECTE;
    if (ip_client) {
        strncpy(ip, ip_client, INET_ADDRSTRLEN);
    }
}

/// <summary>
/// Cierra la conexión del cliente si está abierta; cierra el descriptor de socket, marca el socket como inactivo y señala que la conexión ya no está ocupada.
/// </summary>
void ConnexioClient::tancarConnexio() {
    if (socket_cli != SOCKET_ATURAT) {
        close(socket_cli);
        socket_cli = SOCKET_ATURAT;
    }
    esta_ocupat = false;
}

