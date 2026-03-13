#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>    
#include <sys/stat.h> 
#include <stdlib.h>   
#include "../ClassServer/Dades.h"

// Funció per mostrar el menú i retornar l'opció
int demanar_operacio() {
    int op;
    printf("\n--- CLIENT FTP ---\n");
    printf("1. Llistar directori (ls)\n");
    printf("2. Canviar de directori (cd)\n");
    printf("3. Descarregar fitxer (get)\n");
    printf("4. Descarregar carpeta (rget)\n");
    printf("5. Registrar nou usuari\n");
    printf("6. Sortir\n");
    printf("Opció: ");
    scanf("%d", &op);
    return op;
}

void demanar_usuari_pwd(ConnectionHeader* header) {
    printf("Usuari: ");
    scanf("%s", header->usuari);
    printf("Contrasenya: ");
    scanf("%s", header->contrasenya);
}

int main() {
    int socket_server;
    struct sockaddr_in server;
    ConnectionHeader header;
    int validacio;
    char buffer_rebut[LEN_PAQUET];

    // 1. Configuració del socket
    socket_server = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(PORT_SERVEI);
    memset(server.sin_zero, 0, 8);

    if (connect(socket_server, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Error connectant al servidor");
        return 1;
    }

    // 2. Flux principal d'operacions
    demanar_usuari_pwd(&header);
    int op = demanar_operacio();

    header.operacio = op;
    header.versio = 1;

    // 1a VEGADA: Enviem per validar usuari
    write(socket_server, &header, sizeof(ConnectionHeader));
    read(socket_server, &validacio, sizeof(int));

    if (validacio == VALID) {
        // 2a VEGADA: Enviem per indicar l'operació real (com l'espera el bucle del servidor)
        write(socket_server, &header, sizeof(ConnectionHeader));

        switch (op) {
        case OP_DIR: {
            long mida_llistat;
            // El servidor fa: write(client->getSocketCli(), &mida, sizeof(long));
            if (read(socket_server, &mida_llistat, sizeof(long)) > 0) {
                long total_rebut = 0;
                while (total_rebut < mida_llistat) {
                    int n = read(socket_server, buffer_rebut, sizeof(buffer_rebut) - 1);
                    if (n <= 0) break;
                    buffer_rebut[n] = '\0';
                    printf("%s", buffer_rebut);
                    total_rebut += n;
                }
            }
            break;
        }

        case OP_CD: {
            char nou_path[LEN_BUFFER];
            printf("Introdueix el nou camí (path): ");
            scanf("%s", nou_path);
            // Enviem el path (el servidor l'espera dins del seu mètode op_cd)
            write(socket_server, nou_path, strlen(nou_path));
            printf("Petició de canvi de directori enviada.\n");
            break;
        }

        case OP_GET: {
            char nom_fitxer[LEN_BUFFER];
            printf("Nom del fitxer a descarregar: ");
            scanf("%s", nom_fitxer);

            write(socket_server, nom_fitxer, strlen(nom_fitxer));

            long mida;
            if (read(socket_server, &mida, sizeof(long)) <= 0 || mida < 0) {
                printf("[ERROR] El fitxer no existeix o error al servidor.\n");
            }
            else {
                int fd_desti = open(nom_fitxer, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                long rebut = 0;
                while (rebut < mida) {
                    int n = read(socket_server, buffer_rebut, sizeof(buffer_rebut));
                    if (n <= 0) break;
                    write(fd_desti, buffer_rebut, n);
                    rebut += n;
                }
                close(fd_desti);
                printf("Fitxer '%s' rebut correctament (%ld bytes).\n", nom_fitxer, mida);
            }
            break;
        }

        case OP_RGET: {
            char nom_dir[LEN_BUFFER];
            printf("Nom de la carpeta a descarregar: ");
            scanf("%s", nom_dir);

            write(socket_server, nom_dir, strlen(nom_dir));

            long mida_tar;
            if (read(socket_server, &mida_tar, sizeof(long)) <= 0 || mida_tar <= 0) {
                printf("[ERROR] No s'ha pogut baixar la carpeta.\n");
            }
            else {
                int fd_temp = open("rebut.tar.gz", O_WRONLY | O_CREAT | O_TRUNC, 0666);
                long rebut_tar = 0;
                while (rebut_tar < mida_tar) {
                    int n = read(socket_server, buffer_rebut, sizeof(buffer_rebut));
                    if (n <= 0) break;
                    write(fd_temp, buffer_rebut, n);
                    rebut_tar += n;
                }
                close(fd_temp);

                // Descomprimir i netejar
                mkdir(nom_dir, 0777);
                char cmd[LEN_BUFFER + 64];
                snprintf(cmd, sizeof(cmd), "tar -xzf rebut.tar.gz -C %s", nom_dir);
                system(cmd);
                unlink("rebut.tar.gz");
                printf("Carpeta '%s' descarregada i descomprimida.\n", nom_dir);
            }
            break;
        }

        case OP_SORTIR:
            printf("Desconnectant del servidor...\n");
            break;

        default:
            printf("Operació no implementada o desconeguda.\n");
            break;
        }

        close(socket_server);
        return 0;
    }
}