#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "../Server/definicions.h"

#define PORT 10235


typedef struct {
    int operacio;
    int versio;
    int len;
} t_header;

int main() {
    int srv;
    struct sockaddr_in server;
    t_header h;
    int validacio, a, b, resultat;

    srv = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(PORT);
    memset(server.sin_zero, 0, 8);

    if (connect(srv, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Error conectant");
        return 1;
    }

    // Preparem una operació de suma (Op 1)
    h.operacio = 1;
    h.versio = 1;
    h.len = 2; // Enviem 2 enters

    write(srv, &h, sizeof(t_header));
    read(srv, &validacio, sizeof(int));

    if (validacio == 0) {
        printf("Suma acceptada. Introdueix dos números: ");
        scanf("%d %d", &a, &b);
        write(srv, &a, sizeof(int));
        write(srv, &b, sizeof(int));

        read(srv, &resultat, sizeof(int));
        printf("Resultat del servidor: %d\n", resultat);
    }
    else {
        printf("Operació no permesa.\n");
    }

    close(srv);
    return 0;
}