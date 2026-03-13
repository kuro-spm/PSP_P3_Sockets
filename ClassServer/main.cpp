#include "ServerCpp.h"
#include <iostream>
#include <signal.h>

ServerCpp servidor;

void capturar_sigint(int sig) {
    printf("\n[INFO] Aturant el servidor per senyal %d...\n", sig);
    servidor.stopServer();
    exit(0);
    //En cas d'emergencia: fuser -k 10235/tcp
}

int main() {
    signal(SIGINT, capturar_sigint);

    

    try {
        //Preparem els recursos (sockets, mutex, etc.)
        servidor.inicialitzar();
		
        servidor.registrar_usuari("alumne", "alumne");
		servidor.registrar_usuari("sara", "prats");
		servidor.registrar_usuari("marc", "brufau");


        // Activem el bucle d'acceptació de clients:
        servidor.runServer();
    }
    catch (const char* msg) {
        std::cerr << "[ERROR CRÍTIC] " << msg << std::endl;
        return 1;
    }

    return 0;
}
