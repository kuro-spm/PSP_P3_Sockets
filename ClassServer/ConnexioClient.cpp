#include "ConnexioClient.h"

/// <summary>
/// Constructor de ConnexioClient que inicializa los miembros internos: establece socket_cli a SOCKET_ATURAT, marca esta_ocupat como false, copia PATH_DEFECTE en path_actual usando strncpy y asegura el terminador nulo, y limpia los buffers usuari e ip.
/// </summary>
ConnexioClient::ConnexioClient() {
    socket_cli = SOCKET_ATURAT;
    esta_ocupat = false;
    strncpy(this->path_actual, PATH_DEFECTE, LEN_PATH - 1);
    this->path_actual[LEN_PATH - 1] = '\0';

    memset(usuari, 0, sizeof(usuari));
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
/// Inicializa la conexión del cliente con el socket proporcionado y la dirección IP. Establece socket_cli, marca esta_ocupat como true, copia PATH_DEFECTE en path_actual usando strncpy y asegura el terminador nulo, y copia ip_client en ip si se proporciona.
/// </summary>
void ConnexioClient::inicialitzar(int socket, const char* ip_client) {
    socket_cli = socket;
    esta_ocupat = true;

    strncpy(this->path_actual, PATH_DEFECTE, LEN_PATH - 1);
    this->path_actual[LEN_PATH - 1] = '\0';

    if (ip_client) {
        strncpy(ip, ip_client, INET_ADDRSTRLEN - 1);
        ip[INET_ADDRSTRLEN - 1] = '\0'; // Assegurem el tancament de la cadena
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

