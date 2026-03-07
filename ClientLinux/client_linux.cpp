#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "../Server/definicions.h"
#include "../ClassServer/Dades.h"

int demanar_operacio() {
	int op;
	printf("Selecciona una operació:\n");
	printf("1. ls\n");
	printf("2. Canviar de directori\n");
	printf("3. Descarregar arxiu\n");
	printf("4. Registrar-se\n");
	printf("5. Sortir\n");
	printf("Opció: ");
	scanf("%d", &op);
	return op;
}

int main() {
	int socket_server;
	struct sockaddr_in server;
	MissatgeHeader header;
	int validacio, resultat;

	socket_server = socket(AF_INET, SOCK_STREAM, 0);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_port = htons(PORT_SERVEI);
	memset(server.sin_zero, 0, 8); //Posar els 8 bytes de padding a 0

	if (connect(socket_server, (struct sockaddr*)&server, sizeof(server)) < 0) {
		perror("Error conectant");
		return 1;
	}

	int op = demanar_operacio();
	//Todo: Demanar usuari i contrasenya
	//Todo: Omplir el header amb les dades de l'usuari i contrasenya
	//TODO: Omplir el header amb les dades necessaries per a fer l'operacio?

	header.operacio = op;
	header.versio = 1;
	header.len = 2; // Enviem 2 enters

	write(socket_server, &header, sizeof(MissatgeHeader));
	read(socket_server, &validacio, sizeof(int));

	if (validacio == VALID) {
		switch (op) {
		case OP_LS:
			printf("Operació LS seleccionada.\n");
			break;
		case OP_CD:
			printf("Operació CD seleccionada.\n");
			//Demanar el nou path al usuari i enviar-lo al servidor
			char nou_path[256];
			printf("Introdueix el nou path: ");
			scanf("%s", nou_path);
			write(socket_server, &nou_path, sizeof(int));
			break;
		case OP_DOWNLOAD:
			printf("Operació DOWNLOAD seleccionada.\n");
			//Demanar el nom del fitxer a descarregar i enviar-lo al servidor
			char nom_fitxer[256];
			printf("Introdueix el nom del fitxer a descarregar: ");
			scanf("%s", nom_fitxer);
			write(socket_server, &nom_fitxer, sizeof(int));
			break;
		case OP_REGISTRE:
			printf("Operació REGISTRE seleccionada.\n");
			//Encarra no s'ha implementat
			//el registre al servidor, així que només imprimim un missatge.En una implementació completa, aquí es demanaria el nom d'usuari i la contrasenya, es construiria el header amb aquesta informació i s'enviaria al servidor per al registre.

			break;
		case OP_SORTIR:
			printf("Operació SORTIR seleccionada.\n");
			break;
		}

		//TODO: Canviar el que ha de llegir el client. Ara només llegim un enter, però depenent de l'operació pot ser que el servidor enviï més dades (com el resultat d'un ls, o el contingut d'un fitxer). Hauríem de llegir aquestes dades i mostrar-les al usuari.
		read(socket_server, &resultat, sizeof(int));
		printf("Resultat del servidor: %d\n", resultat);
	}
	else {
		printf("Operació no permesa.\n");
	}

	close(socket_server);
	return 0;
}