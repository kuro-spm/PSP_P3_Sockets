#pragma once

#include "ServerCpp.h"

// Llibreries estàndard de C per a sockets i fitxers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

// Llibreries per a directoris i fitxers (POSIX)
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <arpa/inet.h>

// No necessites <vector>, <algorithm>, <fstream> ni <filesystem> 
// si estàs treballant amb la API de C (FILE*, open, read, write).

ServerCpp::ServerCpp()
{
	//inicialitzar els atributs a valors segurs
	socket_escolta = SOCKET_ATURAT;
	running = false;
	version = "1.0";
}

ServerCpp::~ServerCpp()
{
	stopServer();
	pthread_mutex_destroy(&semafor_clients);
}

void ServerCpp::inicialitzar()
{
	//inicialitzar semàfor
	//semafor_clients = PTHREAD_MUTEX_INITIALIZER; //fora d'un metode es faria aixi...
	pthread_mutex_init(&semafor_clients, NULL);

	//inicialitzar socket d'escolta
	socket_escolta = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_escolta < 0) {
		perror("Error en crear el socket d'escolta");
		return;
	}
	int opt = 1;
	setsockopt(socket_escolta, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	config_servidor.sin_family = AF_INET;
	config_servidor.sin_port = htons(PORT_SERVEI);
	config_servidor.sin_addr.s_addr = INADDR_ANY;
	memset(config_servidor.sin_zero, 0, 8);
}

void ServerCpp::runServer() {
	char ip_str[INET_ADDRSTRLEN];

	if (bind(socket_escolta, (struct sockaddr*)&config_servidor, sizeof(config_servidor)) < 0) {
		perror("Error al BIND");
		return;
	}

	listen(socket_escolta, 10);
	running = true;
	printf("=== SERVIDOR ACTIU AL PORT %d ===\n", PORT_SERVEI);

	while (running) {
		struct sockaddr_in adreça_client;
		socklen_t mida_adreça = sizeof(adreça_client);

		// El servidor es queda bloquejat aquí fins que arriba algú
		int socket_nou = accept(socket_escolta, (struct sockaddr*)&adreça_client, &mida_adreça);

		if (socket_nou < 0) {
			if (running) perror("Error a l'accept");
			continue;
		}

		// Busquem un forat lliure a la nostra taula de clients
		int posicio = buscarPosicioLliure();

		if (posicio != -1) {
			inet_ntop(AF_INET, &(adreça_client.sin_addr), ip_str, INET_ADDRSTRLEN);

			printf("[INFO] Nova connexió des de %s (Socket %d)\n", ip_str, socket_nou);

			if (ip_ja_connectada(ip_str)) {
				printf("[WARNING] IP %s ja connectada. Rebutjant.\n", ip_str);
				close(socket_nou);
				continue; // Salta al següent accept
			}

			// Inicialitzem l'objecte client de la posició trobada
			clients[posicio].inicialitzar(socket_nou, ip_str);

			ThreadArgs* args = new ThreadArgs();
			args->servidor = this;           // El punter a la instància del servidor
			args->client = &clients[posicio]; // El punter al client específic

			// Creem el fil per gestionar aquest client
			// Nota: Passem el punter al client i el mètode estàtic que veurem ara
			if (pthread_create(clients[posicio].getFilIdPtr(), NULL, ServerCpp::gestio_client, args) != 0) {
				perror("Error creant el fil");
				delete args;
				clients[posicio].tancarConnexio();
			}
		}
		else {
			printf("[AVÍS] Servidor ple. Rebutjant connexió de socket %d\n", socket_nou);
			close(socket_nou);
		}
	}
}

void ServerCpp::stopServer()
{
	running = false;
	if (socket_escolta != SOCKET_ATURAT) {
		close(socket_escolta);
		socket_escolta = SOCKET_ATURAT;
	}
}



void* ServerCpp::gestio_client(void* arg) {
	ThreadArgs* args = (ThreadArgs*)arg;
	ServerCpp* servidor = args->servidor;
	ConnexioClient* cclient = args->client;
	delete args;

	int socket = cclient->getSocketCli();
	ConnectionHeader header;
	int resposta_validacio = NO_VALID;

	// Llegim el header (Lectura 1 segons el protocol binari)
	if (read(socket, &header, sizeof(ConnectionHeader)) > 0) {
		if (servidor->validar_usuari(header.usuari, header.contrasenya)) {
			resposta_validacio = VALID;
			cclient->setUsuari(header.usuari);
		}
		write(socket, &resposta_validacio, sizeof(int));

		if (resposta_validacio == VALID) {
			printf("[LOG] Usuari %s autenticat.\n", header.usuari);
			switch (header.operacio) {
			case OP_LS: servidor->dir_servidor(cclient); break;
			case OP_CD: servidor->cd_path(cclient); break;
			case OP_DOWNLOAD: servidor->download_file(cclient); break;
			case OP_SORTIR: break;
			default: printf("[ERROR] Operació %d no suportada.\n", header.operacio); break;
			}
		}
	}
	printf("[INFO] Tancant connexió del client %d\n", socket);
	cclient->tancarConnexio();
	return NULL;
}

void ServerCpp::finalitzar_connexio_client(ConnexioClient* client)
{
	client->tancarConnexio();
}

/// <summary>
/// Comprueba si una dirección IP ya está conectada en el servidor.
/// </summary>
/// <param name="nova_ip">Cadena C (const char*) que contiene la dirección IP a comprobar.</param>
/// <returns>true si la IP ya está presente entre los clientes conectados; false en caso contrario. La comprobación se realiza con protección de concurrencia mediante el mutex interno (semafor_clients).</returns>
bool ServerCpp::ip_ja_connectada(const char* nova_ip) {
	pthread_mutex_lock(&semafor_clients);

	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (this->clients[i].getEstaOcupat()) {
			if (strcmp(this->clients[i].getIpClient(), nova_ip) == 0) {
				pthread_mutex_unlock(&semafor_clients);
				return true;
			}
		}
	}
	pthread_mutex_unlock(&semafor_clients);
	return false;
}

/// <summary>
/// Busca la primera posición libre en la lista de clientes de forma segura para hilos. Bloquea el mutex semafor_clients mientras recorre el array clients y devuelve el índice de la primera entrada no ocupada.
/// </summary>
/// <returns>El índice de la primera posición libre (0..MAX_CLIENTS-1). Devuelve -1 si no hay ninguna posición libre.</returns>
int ServerCpp::buscarPosicioLliure() {
	pthread_mutex_lock(&semafor_clients); // Protegim la llista amb el mutex
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (!clients[i].getEstaOcupat()) {
			pthread_mutex_unlock(&semafor_clients);
			return i;
		}
	}
	pthread_mutex_unlock(&semafor_clients);
	return -1;
}


/********************** FUNCIONALITATS DEL SERVIDOR **********************/

int ServerCpp::dir_servidor(ConnexioClient* client)
{
	char nom_fitxer[LEN_BUFFER];
	char buffer[LEN_PAQUET];
	char comanda[LEN_BUFFER + 64];
	int fd_txt;
	ssize_t bytes_llegits;

	// 1. Definim el nom del fitxer temporal basat en el socket del client
	snprintf(nom_fitxer, sizeof(nom_fitxer), "ls_client%d.txt", client->getSocketCli());

	// 2. Executem la comanda del sistema per crear el fitxer
	snprintf(comanda, sizeof(comanda), "ls -l %s > %s", client->getPathActual(), nom_fitxer);
	if (system(comanda) != 0) {
		perror("Error executant ls");
		return -1;
	}

	// 3. Obrim el fitxer que acaba de crear "system" per lectura
	// Fem servir 'open' (retorna un int) en lloc de ifstream
	fd_txt = open(nom_fitxer, O_RDONLY);
	if (fd_txt < 0) {
		perror("Error al obrir el fitxer temporal");
		return -1;
	}

	// 4. Llegim del fitxer i escrivim directament al socket del client
	// Fem un bucle: mentres puguem llegir del fitxer, enviem al socket
	while ((bytes_llegits = read(fd_txt, buffer, sizeof(buffer))) > 0) {
		// 'client->getSocketCli()' és el destí, 'buffer' la informació
		if (write(client->getSocketCli(), buffer, bytes_llegits) < 0) {
			perror("Error enviant dades al client");
			break;
		}
	}

	// 5. Netegem: tanquem el fitxer i l'esborrem del disc
	close(fd_txt);
	remove(nom_fitxer);

	printf("[INFO:LS] Llistat enviat al client %d\n", client->getSocketCli());
	return 0;
}

/// <summary>
/// Lee una ruta enviada por el cliente y, si existe y es un directorio, actualiza la ruta actual asociada al cliente. Asegura la terminación nula de la cadena leída, comprueba la existencia con opendir, cierra el DIR y registra información o errores.
/// </summary>
/// <param name="client">Puntero a ConnexioClient que representa la conexión del cliente. Se emplea para leer la nueva ruta desde su socket (mediante getSocketCli()) y para almacenar la ruta actualizada (setPathActual()).</param>
/// <returns>0 si la ruta se actualizó correctamente; -1 en caso de error (por ejemplo, fallo de lectura o si el directorio no existe).</returns>
int ServerCpp::cd_path(ConnexioClient* client) {
	char nou_path[LEN_BUFFER];
	ssize_t rebut = read(client->getSocketCli(), nou_path, sizeof(nou_path) - 1);

	if (rebut > 0) {
		nou_path[rebut] = '\0'; // Assegurem que la cadena estigui tancada
		DIR* dir = opendir(nou_path);
		if (dir == NULL) {
			perror("Error al canviar de directori");
			return -1;
		}
		closedir(dir); // IMPORTANT: Tancar el DIR si s'ha obert correctament
		client->setPathActual(nou_path);
		printf("[INFO:CD] Client %d canviat a: %s\n", client->getSocketCli(), client->getPathActual());
		return 0;
	}
	return -1; // Retornar error si no s'ha llegit res
}

int ServerCpp::download_file(ConnexioClient* client)
{
	// Descarrega un fitxer a la màquina del client amb el mateix nom que té en el servidor.
	return 0;
}

int ServerCpp::rget_directory(ConnexioClient* client)
{
	// Descarrega tota la carpeta nom_directori amb tot el seu contingut recursivament. 
	// Es construeix un arbre en el client amb els mateixos arxius que en el servidor. 
	// L'arrel de la estructura en el client serà nom_directori.
	//Farem servir TARGZ de la carpeta. El client ho desempaquetarà i crearà l'arbre automàticament.
	return 0;
}

//********************** Gestió d'usuaris **********************


unsigned long ServerCpp::xifrar_password(const char* password) {
	unsigned long hash = 5381;
	int c;
	while ((c = *password++)) {
		hash = ((hash << 5) + hash) + c;
	}
	return hash;
}

/// <summary>
/// Comprueba si un usuario existe en el archivo 'usuaris.txt'.
/// </summary>
/// <param name="username">Nombre de usuario a buscar. La comparación es exactamente igual (sensible a mayúsculas/minúsculas).</param>
/// <returns>true si el usuario se encuentra en el archivo; false si no se encuentra o si no se puede abrir el archivo.</returns>
bool ServerCpp::existeix_usuari(const char* username)
{
	FILE* fitxer = fopen("usuaris.txt", "r");
	if (fitxer == NULL) {
		return false;
	}
	char linia[128];
	char usr_fitxer[LEN_USUARI + 1];
	unsigned long pwd_fitxer;
	bool trobat = false;
	while (fgets(linia, sizeof(linia), fitxer)) {
		if (sscanf(linia, "%[^:]:%lu", usr_fitxer, &pwd_fitxer) == 2) {
			if (strcmp(username, usr_fitxer) == 0)
				trobat = true;
			break;
		}
	}
	fclose(fitxer);
	return trobat;
}

/// <summary>
/// Registra un nuevo usuario: comprueba si ya existe, cifra la contraseña y la anexa al fichero de usuarios.
/// </summary>
/// <param name="username">Nombre de usuario a registrar.</param>
/// <param name="password">Contraseña en texto plano; se cifra internamente antes de almacenarla.</param>
int ServerCpp::registrar_usuari(const char* username, const char* password) {
	if (existeix_usuari(username)) {
		printf("[ERROR] L'usuari '%s' ja existeix.\n", username);
		return -1;
	}
	pthread_mutex_lock(&semafor_clients);

	FILE* fitxer = fopen("usuaris.txt", "a");
	if (fitxer == NULL) {
		perror("[ERROR] Error obrint usuaris.txt");
		pthread_mutex_unlock(&semafor_clients);
		return -2;
	}

	unsigned long hash_pwd = xifrar_password(password);
	fprintf(fitxer, "%s:%lu\n", username, hash_pwd);
	fclose(fitxer);
	pthread_mutex_unlock(&semafor_clients);

	printf("[INFO] Usuari registrat correctament: %s\n", username);
	return 0;
}

/// <summary>
/// Valida un usuario comprobando el nombre y el hash de la contraseña almacenados en el fichero 'usuaris.txt'.
/// </summary>
/// <param name="usr">Nombre de usuario a validar.</param>
/// <param name="pwd">Contraseña en texto plano; se aplica xifrar_password y se compara el hash con el almacenado.</param>
/// <returns>true si se encuentra una entrada con el mismo usuario y hash de contraseña; false en caso contrario o si no se puede abrir el fichero.</returns>
bool ServerCpp::validar_usuari(const char* usr, const char* pwd) {
	FILE* fitxer = fopen("usuaris.txt", "r");
	if (fitxer == NULL) return false;

	char linia[128];
	char usr_fitxer[LEN_USUARI + 1];
	unsigned long pwd_fitxer;
	bool trobat = false;

	unsigned long hash_rebut = xifrar_password(pwd);

	while (fgets(linia, sizeof(linia), fitxer)) {
		if (sscanf(linia, "%[^:]:%lu", usr_fitxer, &pwd_fitxer) == 2) {
			if (strcmp(usr, usr_fitxer) == 0 && hash_rebut == pwd_fitxer) {
				trobat = true;
				break;
			}
		}
	}
	fclose(fitxer);
	return trobat;
}

