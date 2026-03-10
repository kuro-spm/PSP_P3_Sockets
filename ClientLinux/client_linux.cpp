#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
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

void demanar_usuari_pwd(ConnectionHeader* header) {
	//Demanar usuari i contrasenya
	printf("Usuari: ");
	scanf("%s", header->usuari);
	printf("Contrasenya: ");
	scanf("%s", header->contrasenya);
}

//TODO: Revisar three way handshake i validació de l'usuari abans d'enviar les dades de l'operació al servidor. 
//En una implementació completa, després de connectar-se al servidor, 
// el client hauria d'enviar un missatge de validació amb l'usuari i contrasenya, 
// i esperar la resposta del servidor abans de demanar l'operació a realitzar. 
// Si la validació és correcta, llavors es demana l'operació i s'envia al servidor. Si no, es tanca la connexió.

//TODO: Revisar la implementació de quantes dades ha de llegir el client després d'enviar l'operació al servidor.

int main() {
	int socket_server;
	struct sockaddr_in server;
	ConnectionHeader header;
	int validacio;
	char buffer[LEN_BUFFER];
	char buffer_rebut[LEN_BUFFER*4];

	socket_server = socket(AF_INET, SOCK_STREAM, 0);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_port = htons(PORT_SERVEI);
	memset(server.sin_zero, 0, 8); //Posar els 8 bytes de padding a 0

	if (connect(socket_server, (struct sockaddr*)&server, sizeof(server)) < 0) {
		perror("Error conectant");
		return 1;
	}

	//Omplir el header amb les dades de l'usuari i contrasenya, i demanar l'operació a realitzar
	int op = demanar_operacio();
	demanar_usuari_pwd(&header);

	header.operacio = op;
	header.versio = 1;
	header.len = LEN_BUFFER;
	
	//TODO: Omplir el header amb les dades necessaries per a fer l'operacio?
	switch(op) {
		case OP_LS:
			header.len = 0; // No enviem dades addicionals
			break;
		case OP_CD:
			// El nou path es demanarà després de validar l'usuari
			break;
		case OP_DOWNLOAD:
			// El nom del fitxer es demanarà després de validar l'usuari
			break;
		case OP_REGISTRE:
			// Encarra no s'ha implementat el registre, així que no calen dades addicionals
			break;
		case OP_SORTIR:
			// No calen dades addicionals per a sortir
			header.len = 0;
			break;
		default:
			printf("Operació no reconeguda.\n");
			close(socket_server);
			return 1;
	}
	
	// Enviem el header al servidor
	write(socket_server, &header, sizeof(ConnectionHeader));
	// Esperar la validació del servidor
	read(socket_server, &validacio, sizeof(int));

	if (validacio == VALID) {
		switch (op) {
		case OP_LS:
			printf("Operació LS seleccionada.\n");
			int n;
			printf("--- LLISTAT DEL SERVIDOR ---\n");
			// Llegim mentre el servidor ens enviï dades
			while ((n = read(socket_server, buffer_rebut, sizeof(buffer_rebut) - 1)) > 0) {
				buffer_rebut[n] = '\0';
				printf("%s", buffer_rebut);
				// Si el servidor envia un caràcter buit, hem acabat
				if (buffer_rebut[n - 1] == '\0') break;
			}
			break;
		case OP_CD:
			printf("Operació CD seleccionada.\n");
			//Demanar el nou path al usuari i enviar-lo al servidor
			char nou_path[LEN_BUFFER];
			printf("Introdueix el nou path: ");
			scanf("%s", nou_path);
			write(socket_server, &nou_path, LEN_BUFFER);
			break;
		case OP_DOWNLOAD:
			printf("Operació DOWNLOAD seleccionada.\n");
			//Demanar el nom del fitxer a descarregar i enviar-lo al servidor
			char nom_fitxer[LEN_BUFFER];
			printf("Introdueix el nom del fitxer a descarregar: ");
			scanf("%s", nom_fitxer);
			write(socket_server, &nom_fitxer, LEN_BUFFER);
			break;
		case OP_REGISTRE:
			printf("Operació REGISTRE seleccionada.\n");
			//Encarra no s'ha implementat
			//el registre al servidor, així que només imprimim un missatge.En una implementació completa, aquí es demanaria el nom d'usuari i la contrasenya, es construiria el header amb aquesta informació i s'enviaria al servidor per al registre.
			printf("Funcionalitat de registre no implementada.\n");
			break;
		case OP_SORTIR:
			printf("Operació SORTIR seleccionada.\n");
			break;
		default:
			printf("Operació no reconeguda.\n");
		}
		//Revisar on cal realment llegir dades i amb quina mida després d'enviar l'operació al servidor. En una implementació completa, el client hauria de llegir la resposta del servidor després de cada operació per mostrar el resultat o els errors que puguin ocórrer.
		read(socket_server, &buffer, LEN_BUFFER);
		printf("Resultat del servidor: %s\n", buffer);
	}
	else {
		printf("No s'ha pogut validar.\n");
	}

	close(socket_server);
	return 0;
}