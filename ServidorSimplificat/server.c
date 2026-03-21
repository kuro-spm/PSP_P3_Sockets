#include <stdio.h>              // printf(), perror()
#include <sys/socket.h>         // socket(), bind(), listen(), accept(), connect()
#include <netinet/in.h>         // struct sockaddr_in
#include <arpa/inet.h>          // inet_addr()
#include <string.h>             // strcmp()
#include <unistd.h>             // read(), write(), close()
#include <pthread.h>            // pthread_create(), pthread_join()
#include <stdlib.h>             // exit()


// Definició de constants. Nota: Seria millor fer l'include de "Dades.h" però per evitar confusions i mantenir aquest codi autònom, les definim aquí directament.
#define PORT_SERVEI 10235
#define SOCKET_ATURAT -1
#define VALID 1
#define NO_VALID 0
#define LEN_USUARI 30
#define LEN_CONTRASENYA 30
#define LEN_PAQUET 1024
#define LEN_PATH 512
#define LEN_BUFFER 256
#define MAX_CLIENTS 20

//Errors
#define ERR_CON -1
#define ERR_AUTH -2
#define ERR_ -3


#define FTP_ROOT "./ftp_root"  // Aquest serà el límit màxim del client
#define PATH_DEFECTE "/"       // El client veurà l'arrel com "/"

// Definició de codis d'operació
enum CodisOperacio {
	OP_SORTIR = 99,
	OP_DIR = 1,
	OP_CD = 2,
	OP_GET = 3,
	OP_RGET = 4,
	OP_REGISTRE = 5,
	NUM_OPERACIONS // Aquest valor no s'utilitza com a operació real, sinó com a límit superior (té valor de l'ultim+1)
};

// Estructura del protocold
typedef struct {
	int operacio;
	int versio;
	char path_actual[LEN_PATH];
	int len;
} t_header;

typedef struct {
	int socket;
	pthread_t th;
	int ocupat;
} t_client;

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

//---------------Funcions auxiliars---------------
//------------------------------------------------

void init_clients(t_client* clients) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		clients[i].socket = SOCKET_ATURAT;
		clients[i].ocupat = 0;
	}
}

int pos_taula_clients(t_client* clients) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (!clients[i].ocupat) return i;
	}
	return -1;
}

//--------------Funcions del servidor--------------
//-------------------------------------------------




//----------Funcions de gestió de clients----------
//-------------------------------------------------

void* finalitza_client(void* arg, int err) {
	t_client* client = (t_client*)arg;
	pthread_mutex_lock(&mut);
	client->ocupat = 0;
	client->socket = SOCKET_ATURAT;
	pthread_mutex_unlock(&mut);
	return NULL;
}

void* gestio_client(void* arg) {
	t_client* client = (t_client*)arg;
	t_header header;
	int resp;

	//Llegir Header inicial
	if ((resp = read(client->socket, &header, sizeof(t_header))) < 0) {
		perror("Error llegint header");
		return finalitza_client(arg, ERR_CON);

	}


}




int main()
{
	t_client clients[MAX_CLIENTS];
	int server_socket, client_socket;
	struct sockaddr_in server_addr, client_addr;

	// 1. Crear socket del servidor
	// AF_INET: IPv4, 
	// SOCK_STREAM: TCP, 
	// 0: protocol per defecte
	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Error creant socket");
		exit(EXIT_FAILURE);
	}

	// 2. Configurar adreça del servidor
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY; // Accepta connexions a qualsevol IP local
	server_addr.sin_port = htons(PORT_SERVEI); // Convertir port a format de xarxa
	for (int i = 0; i < 8; i++) {
		server_addr.sin_zero[i] = 0; // Omplir amb zeros
	}

	// Preparar taula clients
	init_clients(clients);

	// 3. Enllaçar socket a l'adreça
	if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		perror("Error enllaçant socket");
		close(server_socket);
		exit(EXIT_FAILURE);
	}

	// 4. Escoltar connexions entrants
	if (listen(server_socket, MAX_CLIENTS) < 0) {
		perror("Error listen");
		close(server_socket);
		exit(EXIT_FAILURE);
	}

	// 5. Bucle principal per acceptar clients
	socklen_t client_len = sizeof(client_addr);

	while (1) {
		if ((client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len)) < 0) {
			perror("Error acceptant connexió");
			continue; // Continuar acceptant altres connexions
		}
		else {
			// Buscar posició lliure a la taula de clients
			int pos = pos_taula_clients(clients);
			if (pos < 0) {
				printf("Servidor ple. Rebutjant connexió.\n");
				close(client_socket);
				continue;
			}
			else {
				// Assignar client a la taula
				pthread_mutex_lock(&mut);
				clients[pos].socket = client_socket;
				clients[pos].ocupat = 1;
				pthread_mutex_unlock(&mut);

				// Crear thread per gestionar el client
				//guardem el thread, li diem quina funcio ha d'executar i li passem la referencia al client que hem guardat a la taula
				if (pthread_create(&clients[pos].th, NULL, gestio_client, &clients[pos]) != 0) {
					perror("Error creant thread");
					close(client_socket);
					finalitza_client(&clients[pos], ERR_CON);
				}
				else {
					pthread_detach(clients[pos].th); // Desprendrem el thread, no necessitem fer pthread_join
				}
			}
		}
	}

	return 0;
}