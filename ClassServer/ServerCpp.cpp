
#include "ServerCpp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <arpa/inet.h>

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

/// <summary>
/// Inicializa el servidor: asegura la existencia del directorio raíz FTP, inicializa el mutex de clientes, crea y configura el socket de escucha y rellena la estructura de configuración del servidor.
/// </summary>
void ServerCpp::inicialitzar()
{
	char comanda_mkdir[128];
	snprintf(comanda_mkdir, sizeof(comanda_mkdir), "mkdir -p %s", FTP_ROOT);

	if (system(comanda_mkdir) != 0) {
		throw "[ERROR] No s'ha pogut assegurar l'existència del directori arrel";
	}
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

/// <summary>
/// Inicia y ejecuta el bucle principal del servidor: enlaza el socket de escucha, pone el socket a escuchar, acepta conexiones entrantes, registra y valida clientes, y crea hilos para gestionar cada conexión. Rechaza conexiones de IP ya conectadas o cuando la tabla de clientes está llena.
/// </summary>
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

	// 1. LLEGIM L'ÚNIC HEADER
	if (read(socket, &header, sizeof(ConnectionHeader)) <= 0) {
		cclient->tancarConnexio();
		return NULL;
	}

	// 2. VALIDACIÓ D'USUARI
	int resposta = servidor->validar_usuari(header.usuari, header.contrasenya) ? VALID : NO_VALID;

	// Si l'operació és de registre, no validem l'usuari primer (perquè encara no existeix!)
	// Per tant, només enviem la resposta de validació per a les altres operacions.
	if (header.operacio != OP_REGISTRE) {
		write(socket, &resposta, sizeof(int));
	}

	if (resposta == VALID || header.operacio == OP_REGISTRE) {
		cclient->setUsuari(header.usuari);
		cclient->setPathActual(header.path_actual);

		printf("[LOG] Usuari %s a %s sol·licita OP %d\n", cclient->getUsuari(), cclient->getPathActual(), header.operacio);

		// 4. EXECUCIÓ ÚNICA
		switch (header.operacio) {
		case OP_DIR:  servidor->op_dir(cclient);  break;
		case OP_CD:   servidor->op_cd(cclient);   break;
		case OP_GET:  servidor->op_get(cclient);  break;
		case OP_RGET: servidor->op_rget(cclient); break;

		case OP_REGISTRE: {
			int res = servidor->registrar_usuari(header.usuari, header.contrasenya);
			int resposta_registre = (res == 0) ? VALID : NO_VALID;
			write(socket, &resposta_registre, sizeof(int));
			break;
		}

		default: break;
		}
	}

	// 5. FINALITZACIÓ
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

int ServerCpp::op_dir(ConnexioClient* client) {
	char nom_fitxer_temporal[LEN_BUFFER];
	char buffer[LEN_PAQUET];
	char ruta_real[LEN_PATH];
	struct stat st;

	// 1. Obtenim la ruta real (ROOT + path virtual)
	construir_ruta_real(client, NULL, ruta_real);

	// 2. Generem un nom de fitxer temporal únic per socket
	snprintf(nom_fitxer_temporal, sizeof(nom_fitxer_temporal), "ls_%d.txt", client->getSocketCli());

	// 3. Calculem la mida de la comanda usant les variables definides
	// Sumem LEN_PATH (ruta) + LEN_BUFFER (fitxer) + caràcters extres de la comanda
	char comanda[LEN_PATH + LEN_BUFFER + 32];
	snprintf(comanda, sizeof(comanda), "ls -l %s > %s", ruta_real, nom_fitxer_temporal);

	printf("[INFO:LS] Executant: %s\n", comanda);
	system(comanda);

	// 4. Obrim el fitxer i enviem la mida primer
	int fd = open(nom_fitxer_temporal, O_RDONLY);
	if (fd >= 0) {
		if (fstat(fd, &st) == 0) {
			long mida = st.st_size;
			// Enviem la mida (8 bytes) perquè el client sàpiga quant llegir
			write(client->getSocketCli(), &mida, sizeof(long));

			ssize_t n;
			while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
				write(client->getSocketCli(), buffer, n);
			}
		}
		close(fd);
		unlink(nom_fitxer_temporal); // Esborrem el temporal
	}
	return 0;
}

int ServerCpp::op_cd(ConnexioClient* client) {
	char nou_path_virtual[LEN_PATH];
	char ruta_real_proposada[LEN_PATH + 64];
	int resultat = NO_VALID;

	if (read(client->getSocketCli(), nou_path_virtual, LEN_PATH - 1) > 0) {
		nou_path_virtual[strlen(nou_path_virtual)] = '\0';

		char path_aux[LEN_PATH + 32];

		// GESTIÓ DEL ".." (Pujar nivell)
		if (strcmp(nou_path_virtual, "..") == 0) {
			if (strcmp(client->getPathActual(), "/") == 0) {
				// Ja som a l'arrel virtual, no podem pujar més
				resultat = NO_VALID;
			}
			else {
				// Copiem el path actual i busquem l'última barra per tallar
				strncpy(path_aux, client->getPathActual(), sizeof(path_aux) - 1);
				char* darrera_barra = strrchr(path_aux, '/');
				if (darrera_barra != NULL) {
					if (darrera_barra == path_aux) {
						// Estàvem a "/carpeta", passem a "/"
						strcpy(path_aux, "/");
					}
					else {
						*darrera_barra = '\0';
					}
					resultat = VALID;
				}
			}
		}
		// GESTIÓ DE RUTES NORMAL 
		else if (strstr(nou_path_virtual, "..") == NULL) {
			if (nou_path_virtual[0] != '/') {
				snprintf(path_aux, sizeof(path_aux), "%s/%s", client->getPathActual(), nou_path_virtual);
			}
			else {
				strncpy(path_aux, nou_path_virtual, sizeof(path_aux) - 1);
			}

			// Comprovem si el directori existeix realment
			snprintf(ruta_real_proposada, sizeof(ruta_real_proposada), "%s%s", FTP_ROOT, path_aux);
			DIR* dir = opendir(ruta_real_proposada);
			if (dir) {
				closedir(dir);
				resultat = VALID;
			}
		}

		// Si el resultat és VALID, el servidor NO actualitza el client aquí (recorda que és transaccional)
		// Només enviem el OK/ERROR. El client s'actualitzarà ell mateix si rep VALID.
		write(client->getSocketCli(), &resultat, sizeof(int));
	}
	return (resultat == VALID) ? 0 : -1;
}
int ServerCpp::op_get(ConnexioClient* client)
{
	char nom_fitxer[LEN_BUFFER];
	char ruta_real[LEN_PATH];
	char buffer[LEN_PAQUET];
	struct stat st;

	if (read(client->getSocketCli(), nom_fitxer, sizeof(nom_fitxer) - 1) <= 0) return -1;
	nom_fitxer[strlen(nom_fitxer)] = '\0';

	// Seguretat contra ".."
	if (strstr(nom_fitxer, "..")) return -1;

	// Convertim la petició del client en una ruta real al servidor
	construir_ruta_real(client, nom_fitxer, ruta_real);

	int fd_fitxer = open(ruta_real, O_RDONLY);
	if (fd_fitxer < 0) {
		long error = -1;
		write(client->getSocketCli(), &error, sizeof(long));
		return -2;
	}

	fstat(fd_fitxer, &st);
	long mida = st.st_size;
	write(client->getSocketCli(), &mida, sizeof(long));

	ssize_t llegits;
	while ((llegits = read(fd_fitxer, buffer, sizeof(buffer))) > 0) {
		write(client->getSocketCli(), buffer, llegits);
	}

	close(fd_fitxer);
	return 0;
}

int ServerCpp::op_rget(ConnexioClient* client)
{
	char nom_carpeta[LEN_BUFFER];
	char fitxer_tar[LEN_BUFFER + 64];
	char comanda[LEN_PATH * 2];
	char ruta_real_carpeta[LEN_PATH];
	struct stat st;

	// 1. Llegir nom de la carpeta
	if (read(client->getSocketCli(), nom_carpeta, sizeof(nom_carpeta) - 1) <= 0) return -1;
	nom_carpeta[strlen(nom_carpeta)] = '\0';

	if (strstr(nom_carpeta, "..")) return -1;

	construir_ruta_real(client, nom_carpeta, ruta_real_carpeta);
	snprintf(fitxer_tar, sizeof(fitxer_tar), "temp_rget_%d.tar.gz", client->getSocketCli());

	// 2. Comprimir
	snprintf(comanda, sizeof(comanda), "tar -czf %s -C %s . 2>/dev/null", fitxer_tar, ruta_real_carpeta);
	if (system(comanda) != 0) {
		long error = -1;
		write(client->getSocketCli(), &error, sizeof(long));
		return -2;
	}

	// 3. Enviar mida i dades
	int fd_tar = open(fitxer_tar, O_RDONLY);
	if (fd_tar >= 0 && fstat(fd_tar, &st) == 0) {
		long mida = (long)st.st_size; // Forcem cast a long
		write(client->getSocketCli(), &mida, sizeof(long));

		char buffer[LEN_PAQUET];
		ssize_t n;
		while ((n = read(fd_tar, buffer, sizeof(buffer))) > 0) {
			write(client->getSocketCli(), buffer, n);
		}
		close(fd_tar);
	}

	unlink(fitxer_tar);
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
			if (strcmp(username, usr_fitxer) == 0) {
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

//********************** Altres mètodes auxiliars **********************/

/// <summary>
/// Construye la ruta real en el sistema de archivos combinando el directorio raíz FTP, la ruta actual del cliente y, opcionalmente, un nombre de fichero.
/// </summary>
/// <param name="client">Puntero a ConnexioClient que proporciona la ruta actual del cliente (getPathActual()).</param>
/// <param name="nom_fitxer">Nombre del fichero a añadir a la ruta. Si es NULL, la función solo construye la ruta del directorio.</param>
/// <param name="ruta_desti">Buffer de salida donde se escribe la ruta resultante. Debe tener espacio para LEN_PATH bytes; la función utiliza snprintf para formatearla.</param>
void ServerCpp::construir_ruta_real(ConnexioClient* client, const char* nom_fitxer, char* ruta_desti) {
	// Si nom_fitxer és NULL, només volem la ruta del directori actual
	if (nom_fitxer == NULL) {
		snprintf(ruta_desti, LEN_PATH, "%s%s", FTP_ROOT, client->getPathActual());
	}
	else {
		snprintf(ruta_desti, LEN_PATH, "%s%s/%s", FTP_ROOT, client->getPathActual(), nom_fitxer);
	}
}