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
#define IP_SERVER "192.168.68.109"

// Funció per mostrar on s'ha desat realment el fitxer en el teu PC
void mostrar_ruta_local_absoluta(const char* nom_fitxer) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("[OK] Fitxer desat localment a: %s/%s\n", cwd, nom_fitxer);
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
    if (chdir(CARPETA_DESCARREGUES) != 0) {
        perror("[ERROR] No s'ha pogut accedir a la carpeta local");
        return 1;
    }

    char ruta_verificacio[1024];
    getcwd(ruta_verificacio, sizeof(ruta_verificacio));
    printf("[SISTEMA] Estàs treballant a: %s\n", ruta_verificacio);

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

            // Enviem bloc de 256 bytes per sincronitzar amb ServerCpp.cpp:274
            write(sock, nou_dir, LEN_PATH);

            int ok;
            read(sock, &ok, sizeof(int));
            if (ok == VALID) {
                // Actualitzem només la referència visual per al menú
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
            memset(fitxer, 0, LEN_BUFFER);
            printf("Fitxer a baixar del servidor: ");
            scanf("%s", fitxer);

            // Enviem bloc de 256 bytes per sincronitzar amb ServerCpp.cpp:313
            write(sock, fitxer, LEN_BUFFER);

            long long mida_f;
            if (read(sock, &mida_f, sizeof(long long)) > 0 && mida_f >= 0) {
                int fd = open(fitxer, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                long long r = 0;
                while (r < mida_f) {
                    int n = read(sock, buffer_rebut, sizeof(buffer_rebut));
                    if (n <= 0) break;
                    write(fd, buffer_rebut, n);
                    r += n;
                }
                close(fd);
                mostrar_ruta_local_absoluta(fitxer); // El que demanaves!
            }
            break;
        }

        case OP_RGET: {
            char carpeta[LEN_BUFFER];
            memset(carpeta, 0, LEN_BUFFER);
            printf("Carpeta a baixar (servidor): ");
            scanf("%s", carpeta);

            write(sock, carpeta, LEN_BUFFER);

            long long mida_t;
            if (read(sock, &mida_t, sizeof(long long)) > 0 && mida_t > 0) {
                int fd = open("rebut.tar.gz", O_WRONLY | O_CREAT | O_TRUNC, 0666);
                long long r = 0;
                while (r < mida_t) {
                    int n = read(sock, buffer_rebut, sizeof(buffer_rebut));
                    if (n <= 0) break;
                    write(fd, buffer_rebut, n);
                    r += n;
                }
                close(fd);
                mkdir(carpeta, 0777);
                char cmd[512];
                snprintf(cmd, sizeof(cmd), "tar -xzf rebut.tar.gz -C %s 2>/dev/null", carpeta);
                system(cmd);
                unlink("rebut.tar.gz");
                mostrar_ruta_local_absoluta(carpeta); // També aquí!
            }
            break;
        }
        }
        close(sock);
    }
    return 0;
}