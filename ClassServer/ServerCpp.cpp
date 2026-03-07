#include "ServerCpp.h"
#include <vector>
#include <algorithm>
#include <fstream>

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
			char ip_str[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(adreça_client.sin_addr), ip_str, INET_ADDRSTRLEN);

			printf("[INFO] Nova connexió des de %s (Socket %d)\n", ip_str, socket_nou);

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
	MissatgeHeader header;
	int resposta_validacio = NO_VALID;

	// Llegim el header (Lectura 1 segons el protocol binari)
	if (read(socket, &header, sizeof(MissatgeHeader)) > 0) {
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

void ServerCpp::dir_servidor(ConnexioClient* client)
{
	//Llistarà els arxius del directori actual del client.
	// El resultat del ls/dir s'ha de convertir en un arxiu de text temporal amb un identificador del client, el qual s'enviarà al client.
}

void ServerCpp::cd_path(ConnexioClient* client) {
	char nou_path[256];
	// Llegim el path des del socket del client
	if (read(client->getSocketCli(), nou_path, sizeof(nou_path)) > 0) {
		// çcomprovar si el path existeix amb opendir()
		DIR* dir = opendir(nou_path);
		if (dir == NULL) {
			perror("Error al canviar de directori");
			return;
		}
		client->setPathActual(nou_path);
		printf("[INFO:CD] Client %d canviat a: %s\n", client->getSocketCli(), client->getPathActual());
	}
}

void ServerCpp::download_file(ConnexioClient* client)
{
	// Descarrega un fitxer a la màquina del client amb el mateix nom que té en el servidor.

}

void ServerCpp::rget_directory(ConnexioClient* client)
{
	// Descarrega tota la carpeta nom_directori amb tot el seu contingut recursivament. 
	// Es construeix un arbre en el client amb els mateixos arxius que en el servidor. 
	// L'arrel de la estructura en el client serà nom_directori.
	//Farem servir TARGZ de la carpeta. El client ho desempaquetarà i crearà l'arbre automàticament.

}


/// <summary>
/// Calcula un valor hash de la contraseña usando una variante del algoritmo DJB2. No es un método criptográfico seguro para almacenar contraseñas.
/// </summary>
/// <param name="password">La contraseña a convertir en hash (pasada como referencia constante a std::string).</param>
/// <returns>Un valor de tipo unsigned long que representa el hash calculado de la contraseña.</returns>
unsigned long ServerCpp::xifrar_password(const std::string& password) {
	unsigned long hash = 5381;
	for (char c : password) {
		hash = ((hash << 5) + hash) + (unsigned char)c;
	}
	return hash;
}

/// <summary>
/// Comprueba si un usuario existe en el archivo 'usuaris.txt'.
/// </summary>
/// <param name="username">Nombre de usuario a buscar. La comparación es exactamente igual (sensible a mayúsculas/minúsculas).</param>
/// <returns>true si el usuario se encuentra en el archivo; false si no se encuentra o si no se puede abrir el archivo.</returns>
bool ServerCpp::existeix_usuari(const std::string& username)
{
	FILE* fitxer = fopen("usuaris.txt", "r");
	if (fitxer == NULL) {
		return false;
	}
	char linia[128];
	char usr_fitxer[20];
	unsigned long pwd_fitxer;
	bool trobat = false;
	while (fgets(linia, sizeof(linia), fitxer)) {
		if (sscanf(linia, "%[^:]:%lu", usr_fitxer, &pwd_fitxer) == 2) {
			if (username == usr_fitxer) {
				trobat = true;
				break;
			}
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
void ServerCpp::registrar_usuari(const std::string& username, const std::string& password) {
	if (existeix_usuari(username)) {
		printf("[ERROR] L'usuari '%s' ja existeix.\n", username.c_str());
		return;
	}
	pthread_mutex_lock(&semafor_clients);

	FILE* fitxer = fopen("usuaris.txt", "a");
	if (fitxer == NULL) {
		perror("Error obrint usuaris.txt");
		pthread_mutex_unlock(&semafor_clients);
		return;
	}

	unsigned long hash_pwd = xifrar_password(password);
	fprintf(fitxer, "%s:%lu\n", username.c_str(), hash_pwd);

	fclose(fitxer);
	pthread_mutex_unlock(&semafor_clients);

	printf("[INFO] Usuari registrat correctament: %s\n", username.c_str());
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
/// Valida un usuario comprobando el nombre y el hash de la contraseña almacenados en el fichero 'usuaris.txt'.
/// </summary>
/// <param name="usr">Nombre de usuario a validar.</param>
/// <param name="pwd">Contraseña en texto plano; se aplica xifrar_password y se compara el hash con el almacenado.</param>
/// <returns>true si se encuentra una entrada con el mismo usuario y hash de contraseña; false en caso contrario o si no se puede abrir el fichero.</returns>
bool ServerCpp::validar_usuari(const std::string& usr, const std::string& pwd) {
	FILE* fitxer = fopen("usuaris.txt", "r");
	if (fitxer == NULL) return false;

	char linia[128];
	char usr_fitxer[20];
	unsigned long pwd_fitxer;
	bool trobat = false;

	unsigned long hash_rebut = xifrar_password(pwd);

	while (fgets(linia, sizeof(linia), fitxer)) {
		if (sscanf(linia, "%[^:]:%lu", usr_fitxer, &pwd_fitxer) == 2) {
			if (usr == usr_fitxer && hash_rebut == pwd_fitxer) {
				trobat = true;
				break;
			}
		}
	}
	fclose(fitxer);
	return trobat;
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
