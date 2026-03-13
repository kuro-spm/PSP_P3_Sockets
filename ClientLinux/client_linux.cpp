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

// Variable global del client per recordar on es troba (Gaurdià de la memòria)
char path_local[MAX_PATH] = "/";

int main() {
    struct sockaddr_in server_addr;
    ConnectionHeader header;
    char buffer_rebut[LEN_PAQUET];
    bool sistema_actiu = true;

    // Demanem credencials un cop al principi
    demanar_usuari_pwd(&header);

    while (sistema_actiu) {
        int op = demanar_operacio();
        if (op == OP_SORTIR) {
            sistema_actiu = false;
            break;
        }

        // --- INICI DE LA TRANSACCIÓ ---
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT_SERVEI);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Error de connexió");
            break;
        }

        // 1. Enviar Header amb l'operació actual
        header.operacio = op;
        write(sock, &header, sizeof(ConnectionHeader));

        // 2. Rebre validació d'usuari
        int validacio;
        read(sock, &validacio, sizeof(int));
        if (validacio != VALID) {
            printf("Error d'autenticació.\n");
            close(sock);
            break;
        }

        // 3. Enviar el nostre PATH ACTUAL (La nostra memòria)
        write(sock, path_local, strlen(path_local) + 1);

        // 4. Executar lògica de l'operació
        switch (op) {
        case OP_DIR: {
            long mida;
            if (read(sock, &mida, sizeof(long)) > 0) {
                long rebut = 0;
                printf("\n--- Contingut de %s (%ld bytes) ---\n", path_local, mida);
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
            char nou_dir[LEN_BUFFER];
            printf("Directori destí: ");
            scanf("%s", nou_dir);
            write(sock, nou_dir, strlen(nou_dir) + 1);

            int ok;
            read(sock, &ok, sizeof(int));
            if (ok == VALID) {
                // Actualitzem la nostra memòria local
                if (nou_dir[0] == '/') strcpy(path_local, nou_dir);
                else {
                    if (strcmp(path_local, "/") != 0) strcat(path_local, "/");
                    strcat(path_local, nou_dir);
                }
                printf("Directori actualitzat a: %s\n", path_local);
            }
            else {
                printf("Error: El directori no existeix.\n");
            }
            break;
        }

        case OP_GET: {
            char fitxer[LEN_BUFFER];
            printf("Fitxer a descarregar: ");
            scanf("%s", fitxer);
            write(sock, fitxer, strlen(fitxer) + 1);

            long mida_f;
            if (read(sock, &mida_f, sizeof(long)) > 0 && mida_f >= 0) {
                int fd = open(fitxer, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                long total_f = 0;
                while (total_f < mida_f) {
                    int n = read(sock, buffer_rebut, sizeof(buffer_rebut));
                    write(fd, buffer_rebut, n);
                    total_f += n;
                }
                close(fd);
                printf("Fitxer rebut.\n");
            }
            break;
        }
        }

        // --- FINAL DE LA TRANSACCIÓ ---
        close(sock);
    }

    return 0;
}