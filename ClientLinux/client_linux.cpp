#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>    
#include <sys/stat.h> 
#include <stdlib.h>   
#include <limits.h>
#include "../ClassServer/Dades.h"

#define CARPETA_DESCARREGUES "./ftpDownloads"
#define IP_SERVER_NOT_IN_USE "10.2.31.249"
#define IP_SERVER "127.0.0.1"

/*Com executar en WSL :
cd /mnt/c/Users/sprat/Documents/Aa_ICB0/dam2/DAM2_psp/sockets/PracticaSockets
make
./bin_linux/client_linux
*/

void mostrar_ruta_local_absoluta(const char* nom_fitxer) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        // Mostrem la ruta del projecte + la carpeta de descarregues + el fitxer
        printf("[OK] Fitxer desat a: %s/%s/%s\n", cwd, CARPETA_DESCARREGUES, nom_fitxer);
    }
}

void demanar_usuari_pwd(ConnectionHeader* h) {
    printf("--- AUTENTICACIÓ (LINUX) ---\n");
    printf("Usuari: ");
    scanf("%s", h->usuari);
    printf("Contrasenya: ");
    scanf("%s", h->contrasenya);
    h->versio = 1;
    strncpy(h->path_actual, "/", LEN_PATH - 1);
}

int demanar_operacio() {
    int opcio;
    printf("\nSELECCIONA UNA OPERACIÓ:\n");
    printf("%d. Llistar fitxers (ls)\n", OP_DIR);
    printf("%d. Canviar directori SERVIDOR (cd)\n", OP_CD);
    printf("%d. Descarregar fitxer (get)\n", OP_GET);
    printf("%d. Descarregar carpeta (rget)\n", OP_RGET);
    printf("%d. Sortir\n", OP_SORTIR);
    printf("Opcio: ");

    if (scanf("%d", &opcio) != 1) {
        while (getchar() != '\n');
        return -1;
    }
    return opcio;
}

char path_server_virtual[LEN_PATH] = "/";

int main() {
    struct sockaddr_in server_addr;
    ConnectionHeader header;
    char buffer_rebut[LEN_PAQUET];
    bool sistema_actiu = true;

    // 1. Preparar carpeta local (Dolphin la veurà aquí)
    system("mkdir -p " CARPETA_DESCARREGUES);
    
    char ruta_base[PATH_MAX];
    getcwd(ruta_base, sizeof(ruta_base));
    printf("[SISTEMA] Executant-se des de: %s\n", ruta_base);
    printf("[SISTEMA] Les descarregues aniran a: %s/%s\n", ruta_base, CARPETA_DESCARREGUES);

    demanar_usuari_pwd(&header);

    while (sistema_actiu) {
        int op = demanar_operacio();
        if (op == OP_SORTIR) break;

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT_SERVEI);
        server_addr.sin_addr.s_addr = inet_addr(IP_SERVER);

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Error de connexió");
            continue;
        }

        header.operacio = op;
        strncpy(header.path_actual, path_server_virtual, LEN_PATH - 1);
        header.path_actual[LEN_PATH - 1] = '\0';

        // Enviem Header inicial
        write(sock, &header, sizeof(ConnectionHeader));

        int validacio;
        if (read(sock, &validacio, sizeof(int)) <= 0 || validacio != VALID) {
            printf("Error d'autenticació.\n");
            close(sock); continue;
        }

        switch (op) {
        case OP_DIR: {
            long long mida;
            if (read(sock, &mida, sizeof(long long)) > 0) {
                long long rebut = 0;
                printf("\n--- Contingut del SERVIDOR a %s (%lld bytes) ---\n", path_server_virtual, mida);
                while (rebut < mida) {
                    int n = read(sock, buffer_rebut, sizeof(buffer_rebut) - 1);
                    if (n <= 0) break;
                    buffer_rebut[n] = '\0';
                    printf("%s", buffer_rebut);
                    rebut += n;
                }
            }
            break;
        }

        case OP_CD: {
            char nou_dir[LEN_PATH];
            memset(nou_dir, 0, LEN_PATH);
            printf("Nou directori al SERVIDOR: ");
            scanf("%s", nou_dir);

            write(sock, nou_dir, LEN_PATH);

            int ok;
            read(sock, &ok, sizeof(int));
            if (ok == VALID) {
                if (strcmp(nou_dir, "..") == 0) {
                    char* barra = strrchr(path_server_virtual, '/');
                    if (barra != path_server_virtual && barra != NULL) *barra = '\0';
                    else strcpy(path_server_virtual, "/");
                }
                else {
                    if (strcmp(path_server_virtual, "/") != 0) strcat(path_server_virtual, "/");
                    strcat(path_server_virtual, nou_dir);
                }
                printf("[OK] El servidor s'ha mogut a: %s\n", path_server_virtual);
            }
            else {
                printf("[ERR] El servidor no ha pogut canviar de directori.\n");
            }
            break;
        }

        case OP_GET: {
            char fitxer[LEN_BUFFER];
            char ruta_desti[PATH_MAX];
            memset(fitxer, 0, LEN_BUFFER);

            printf("Fitxer a baixar del servidor: ");
            scanf("%s", fitxer);

            // Enviem el nom del fitxer al servidor
            write(sock, fitxer, LEN_BUFFER);

            long long mida_f;
            if (read(sock, &mida_f, sizeof(long long)) > 0 && mida_f >= 0) {
                // Preparem la ruta relativa: "./ftpDownloads/nom_fitxer"
                snprintf(ruta_desti, sizeof(ruta_desti), "%s/%s", CARPETA_DESCARREGUES, fitxer);

                int fd = open(ruta_desti, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd < 0) {
                    perror("[ERR] No s'ha pogut crear el fitxer local");
                    break;
                }

                long long r = 0;
                while (r < mida_f) {
                    int n = read(sock, buffer_rebut, sizeof(buffer_rebut));
                    if (n <= 0) break;
                    write(fd, buffer_rebut, n);
                    r += n;
                }
                close(fd);

                // Cridem la funció per mostrar la ruta absoluta real
                mostrar_ruta_local_absoluta(fitxer);
            }
            else {
                printf("[ERR] El fitxer no existeix al servidor o mida incorrecta.\n");
            }
            break;
        }

        case OP_RGET: {
            char carpeta[LEN_BUFFER];
            char ruta_tar[PATH_MAX];
            memset(carpeta, 0, LEN_BUFFER);

            printf("Carpeta a baixar (servidor): ");
            scanf("%s", carpeta);

            write(sock, carpeta, LEN_BUFFER);

            long long mida_t;
            if (read(sock, &mida_t, sizeof(long long)) > 0 && mida_t > 0) {
                // Ruta on guardarem el tar temporalment
                snprintf(ruta_tar, sizeof(ruta_tar), "%s/rebut.tar.gz", CARPETA_DESCARREGUES);

                int fd = open(ruta_tar, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                long long r = 0;
                while (r < mida_t) {
                    int n = read(sock, buffer_rebut, sizeof(buffer_rebut));
                    if (n <= 0) break;
                    write(fd, buffer_rebut, n);
                    r += n;
                }
                close(fd);

                // Comanda per extreure el contingut DINS de ftpDownloads
                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "tar -xzf %s -C %s/ 2>/dev/null", ruta_tar, CARPETA_DESCARREGUES);
                system(cmd);

                // Esborrem el fitxer temporal .tar.gz
                unlink(ruta_tar);

                mostrar_ruta_local_absoluta(carpeta);
            }
            else {
                printf("[ERR] No s'ha pogut descarregar la carpeta.\n");
            }
            break;
        }
        }
        close(sock);
    }
    return 0;
}