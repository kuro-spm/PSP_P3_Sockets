#include <stdio.h>              // printf(), perror()
#include <sys/socket.h>         // socket(), bind(), listen(), accept(), connect()
#include <netinet/in.h>         // struct sockaddr_in
#include <arpa/inet.h>          // inet_addr()
#include <string.h>             // strcmp()
#include <unistd.h>             // read(), write(), close()
#include <pthread.h>            // pthread_create(), pthread_join()
#include <stdlib.h>             // exit()
#include <dirent.h>     // Per a opendir(), readdir(), closedir()
#include <sys/stat.h>   // Per a stat() i treure la mida dels fitxers
#include <stdlib.h>     // Per a malloc() i free()
#include <sys/wait.h>   // Per a waitpid() i les macros WIFEXITED, etc.
#include <fcntl.h>		// Per a open() i O_RDONLY
#include <limits.h>     // Per a PATH_MAX
#include <signal.h>     // Per capturar Ctrl+C (SIGINT)

// Definició de constants. Nota: Seria millor fer l'include de "Dades.h" però per evitar confusions i mantenir aquest codi autònom, les definim aquí directament.
#define PORT_SERVEI 10235
#define SOCKET_ATURAT -1
#define VALID 1
#define NO_VALID 0
#define LEN_USUARI 30
#define LEN_CONTRASENYA 30
#define LEN_PAQUET 4096
#define LEN_PATH 512
#define LEN_BUFFER 256
#define MAX_CLIENTS 20
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
//Errors
#define ERR_CON -1
#define ERR_AUTH -2
#define ERR_FILE -3

#define FITXER_USUARIS "usuaris.txt"
#define FITXER_TMP "usuaris_tmp.txt"
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
	OP_LOGIN = 6,
	NUM_OPERACIONS // Aquest valor no s'utilitza com a operació real, sinó com a límit superior (té valor de l'ultim+1)
};

// Estructura del protocold
typedef struct {
	int operacio;
	int versio;
	int len;
} t_header;

typedef struct {
	int socket;
	pthread_t th;
	int ocupat;
	int validat;
	char path_actual[LEN_PATH];
} t_client;

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
t_client clients[MAX_CLIENTS];
int server_socket_global = -1;


//---------------Prototipus de Gestió d'Usuaris---------------
unsigned long xifrar_password(const char* password);
int existeix_usuari(const char* username);
int registrar_usuari(const char* username, const char* password);
int validar_usuari(const char* usr, const char* pwd);
void incrementar_comptador(const char* username);

//---------------Prototipus Funcions Servidor-----------------
void construir_ruta_real(t_client* client, const char* afegit, char* ruta_real);
int op_dir(t_client* client);
void op_cd(t_client* client);
int op_get(t_client* client);
int op_rget(t_client* client);


//---------------Funcions auxiliars---------------
//------------------------------------------------
void handler_senyals(int sig) {
	if (sig == SIGINT) {
		printf("\n[INFO] S'ha rebut Ctrl+C. Iniciant tancament segur...\n");

		// 1. Tancar el socket principal (deixa d'acceptar nous clients)
		if (server_socket_global != -1) {
			close(server_socket_global);
			printf("[INFO] Port d'escolta alliberat.\n");
		}

		// 2. Avisar i desconnectar els clients actius
		pthread_mutex_lock(&mut);
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (clients[i].ocupat == 1) {
				printf("[INFO] Forçant desconnexió del client al slot %d...\n", i);
				// Tancar el socket fa que el read() del fil falli i el fil acabi de forma natural
				if (clients[i].socket != SOCKET_ATURAT) {
					close(clients[i].socket);
				}
				// Si algun fil fos molt rebel, es podria forçar amb: pthread_cancel(clients[i].th);
			}
		}
		pthread_mutex_unlock(&mut);

		printf("[INFO] Servidor tancat completament. Adeu!\n");
		exit(0);
	}
}

void init_clients(t_client* clients) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		clients[i].socket = SOCKET_ATURAT;
		clients[i].ocupat = 0;
		clients[i].validat = 0;
		strcpy(clients[i].path_actual, PATH_DEFECTE);
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
void op_cd(t_client* client);
int op_dir(t_client* client);
int op_get(t_client* client);
int op_rget(t_client* client);



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

	while (1) {

		//Llegir Header inicial
		if ((resp = read(client->socket, &header, sizeof(t_header))) < 0) {
			perror("Error llegint header");
			return finalitza_client(arg, ERR_CON);
		}

		if (header.operacio == OP_SORTIR) {
			printf("[LOG] El client ha demanat sortir.\n");
			break;
		}

		if (client->validat == 0 && header.operacio != OP_LOGIN && header.operacio != OP_REGISTRE) {
			printf("[LOG] Intent d'operació sense validar.\n");
			continue;
		}

		switch (header.operacio) {
		case OP_LOGIN: {
			char username[LEN_USUARI + 1];
			char password[LEN_CONTRASENYA + 1];
			// Llegim username i password
			if (read(client->socket, username, LEN_USUARI) <= 0 ||
				read(client->socket, password, LEN_CONTRASENYA) <= 0) {
				perror("Error llegint credencials");
				return finalitza_client(arg, ERR_CON);
			}
			int valid = validar_usuari(username, password);
			write(client->socket, &valid, sizeof(int));
			if (valid == VALID) {
				pthread_mutex_lock(&mut);
				client->validat = 1;
				pthread_mutex_unlock(&mut);
				printf("[LOG] Usuari '%s' validat correctament.\n", username);
			}
			else {
				printf("[LOG] Validació fallida per a l'usuari '%s'.\n", username);
			}
			break;
		}
		case OP_REGISTRE: {
			char username[LEN_USUARI + 1];
			char password[LEN_CONTRASENYA + 1];
			// Llegim username i password
			if (read(client->socket, username, LEN_USUARI) <= 0 ||
				read(client->socket, password, LEN_CONTRASENYA) <= 0) {
				perror("Error llegint credencials");
				return finalitza_client(arg, ERR_CON);
			}
			int res = registrar_usuari(username, password);
			int resposta_registre = (res == VALID) ? VALID : NO_VALID;
			write(client->socket, &resposta_registre, sizeof(int));
			if (res == VALID) {
				printf("[LOG] Nou usuari registrat: '%s'.\n", username);
			}
			else {
				printf("[LOG] Intent de registre fallit per a l'usuari '%s' (ja existeix).\n", username);
			}
			break;
		}
		case OP_DIR: {
			if (op_dir(client) < 0) {
				printf("[ERROR] Error executant OP_DIR.\n");
			}
			break;
		}case OP_CD: {
			op_cd(client);
			break;
		}
		case OP_GET: {
			if (op_get(client) < 0) {
				printf("[ERROR] Error executant OP_GET.\n");
			}
			break;
		}
		case OP_RGET: {
			if (op_rget(client) < 0) {
				printf("[ERROR] Error executant OP_RGET.\n");
			}
			break;
		}
		default:
			printf("[WARNING] Operació desconeguda: %d\n", header.operacio);
			break;
		}

	}
	close(client->socket);
	return finalitza_client(arg, 0);
}










int main()
{
	int server_socket, client_socket;
	struct sockaddr_in server_addr, client_addr;

	signal(SIGINT, handler_senyals);

	// 1. Crear socket del servidor
	// AF_INET: IPv4, 
	// SOCK_STREAM: TCP, 
	// 0: protocol per defecte
	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Error creant socket");
		exit(EXIT_FAILURE);
	}
	server_socket_global = server_socket;

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

	printf("Servidor FTP engegat al port %d. Esperant connexions...\n", PORT_SERVEI);

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

//********************** Gestió d'usuaris **********************
// Implementació de funcions de gestió d'usuaris

unsigned long xifrar_password(const char* password) {
	unsigned long hash = 5381;
	int c;
	while ((c = *password++)) {
		hash = ((hash << 5) + hash) + c;
	}
	return hash;
}

int existeix_usuari(const char* username) {
	pthread_mutex_lock(&mut);
	FILE* fitxer = fopen(FITXER_USUARIS, "r");
	if (!fitxer) {
		pthread_mutex_unlock(&mut);
		return ERR_FILE;
	}

	char linia[128], usr_fitxer[LEN_USUARI + 1];
	int trobat = ERR_AUTH;

	while (fgets(linia, sizeof(linia), fitxer)) {
		// Ens fixem només en el primer camp abans dels ':'
		if (sscanf(linia, "%[^:]", usr_fitxer) == 1 && strcmp(username, usr_fitxer) == 0) {
			trobat = VALID;
			break;
		}
	}
	fclose(fitxer);
	pthread_mutex_unlock(&mut);
	return trobat;
}

int registrar_usuari(const char* username, const char* password) {
	if (existeix_usuari(username) == VALID) return -1; // Usuari ja existeix

	pthread_mutex_lock(&mut);
	FILE* fitxer = fopen(FITXER_USUARIS, "a");
	if (!fitxer) {
		pthread_mutex_unlock(&mut);
		return ERR_FILE;
	}

	fprintf(fitxer, "%s:%lu:0\n", username, xifrar_password(password));

	fclose(fitxer);
	pthread_mutex_unlock(&mut);
	return VALID;
}

int validar_usuari(const char* usr, const char* pwd) {
	pthread_mutex_lock(&mut);
	FILE* fitxer = fopen(FITXER_USUARIS, "r");
	if (!fitxer) {
		pthread_mutex_unlock(&mut);
		return NO_VALID;
	}

	char linia[128], usr_fitxer[LEN_USUARI + 1];
	unsigned long pwd_fitxer;
	unsigned long hash_rebut = xifrar_password(pwd);
	int valid = NO_VALID;

	while (fgets(linia, sizeof(linia), fitxer)) {
		// Llegim usuari i hash. Ignorem la resta (comptador).
		if (sscanf(linia, "%[^:]:%lu", usr_fitxer, &pwd_fitxer) >= 2) {
			if (strcmp(usr, usr_fitxer) == 0 && hash_rebut == pwd_fitxer) {
				valid = VALID;
				break;
			}
		}
	}
	fclose(fitxer);
	pthread_mutex_unlock(&mut);
	return valid;
}

void incrementar_comptador(const char* username) {
	pthread_mutex_lock(&mut);
	FILE* fitxer = fopen(FITXER_USUARIS, "r");
	FILE* temp = fopen(FITXER_TMP, "w");

	if (!fitxer || !temp) {
		if (fitxer) fclose(fitxer);
		if (temp) fclose(temp);
		pthread_mutex_unlock(&mut);
		return;
	}

	char linia[128], u[LEN_USUARI + 1];
	unsigned long h;
	int comptador;

	while (fgets(linia, sizeof(linia), fitxer)) {
		if (sscanf(linia, "%[^:]:%lu:%d", u, &h, &comptador) == 3) {
			if (strcmp(u, username) == 0) comptador++;
			fprintf(temp, "%s:%lu:%d\n", u, h, comptador);
		}
	}

	fclose(fitxer);
	fclose(temp);

	remove(FITXER_USUARIS);
	rename(FITXER_TMP, FITXER_USUARIS);

	pthread_mutex_unlock(&mut);
}
//********************** Funcions Servidor **********************

void construir_ruta_real(t_client* client, const char* afegit, char* ruta_real) {
	char ruta_base[PATH_MAX];
	snprintf(ruta_base, sizeof(ruta_base), "%s%s", FTP_ROOT, client->path_actual);

	if (afegit != NULL && strlen(afegit) > 0) {
		snprintf(ruta_real, PATH_MAX, "%s/%s", ruta_base, afegit);
	}
	else {
		strcpy(ruta_real, ruta_base);
	}
}

int op_dir(t_client* client) {
	char ruta_real[LEN_PATH];
	construir_ruta_real(client, NULL, ruta_real);

	// 1. Obrim el directori de manera segura
	DIR* dir = opendir(ruta_real);
	if (dir == NULL) {
		long long error = -1; // Si no existeix o no hi ha permisos, enviem error
		write(client->socket, &error, sizeof(long long));
		return -1;
	}

	// 2. Preparem un buffer dinàmic a la RAM per guardar el text del directori
	// Posem 64KB de límit, més que suficient per a un directori normal.
	size_t mida_maxima = 65536;
	char* buffer_llista = (char*)malloc(mida_maxima);
	if (!buffer_llista) {
		closedir(dir);
		return -1;
	}
	buffer_llista[0] = '\0'; // Inicialitzem com a string buit

	struct dirent* entrada;
	struct stat info_fitxer;
	char ruta_completa[LEN_PATH];
	char linia[512];

	// 3. Llegim el contingut del directori fitxer a fitxer
	while ((entrada = readdir(dir)) != NULL) {
		// Ignorem els directoris especials "." i ".." per netejar la sortida
		if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0) {
			continue;
		}

		// Obtenim la ruta sencera del fitxer per poder-ne llegir les propietats
		snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", ruta_real, entrada->d_name);

		if (stat(ruta_completa, &info_fitxer) == 0) {
			// Construïm una línia simulant "ls -l" (mida en bytes i nom)
			// Podríem afegir el tipus (directori/fitxer) però així ho mantenim net
			snprintf(linia, sizeof(linia), "%10lld bytes  %s\n",
				(long long)info_fitxer.st_size, entrada->d_name);

			// Ens assegurem de no desbordar el buffer de memòria
			if (strlen(buffer_llista) + strlen(linia) < mida_maxima) {
				strcat(buffer_llista, linia);
			}
		}
	}
	closedir(dir); // Hem acabat de llegir

	// 4. Ara ja tenim tot el text generat i podem saber la mida EXACTA
	long long mida_total = (long long)strlen(buffer_llista);

	// Enviem primer la mida (els 8 bytes del protocol)
	write(client->socket, &mida_total, sizeof(long long));

	// Enviem el contingut sencer de cop!
	if (mida_total > 0) {
		write(client->socket, buffer_llista, mida_total);
	}

	// 5. Neteja: Alliberem la memòria RAM utilitzada
	free(buffer_llista);
	printf("[INFO] Comanda DIR executada de forma nativa a %s\n", ruta_real);

	return 0;
}

int op_rget(t_client* client) {
	char nom_carpeta[LEN_BUFFER];
	char fitxer_tar[LEN_BUFFER + 64];
	char ruta_real_carpeta[LEN_PATH];
	struct stat st;

	// 1. Llegim el nom de la carpeta
	memset(nom_carpeta, 0, LEN_BUFFER);
	if (read(client->socket, nom_carpeta, LEN_BUFFER) <= 0) return -1;

	// 2. Construïm les rutes
	construir_ruta_real(client, nom_carpeta, ruta_real_carpeta);
	snprintf(fitxer_tar, sizeof(fitxer_tar), "temp_rget_%d.tar.gz", client->socket);

	// --- INICI DEL NOU SISTEMA SEGUR DE CREACIÓ DE PROCESSOS ---
	pid_t pid = fork();

	if (pid < 0) {
		// Error crític: no s'ha pogut fer el fork
		perror("Error fent fork");
		long long error = -1;
		write(client->socket, &error, sizeof(long long));
		return -1;
	}
	else if (pid == 0) {
		// PROCÉS FILL: Aquest clon es sacrificarà per convertir-se en 'tar'

		// Opcional: Silenciem els errors del tar (l'equivalent a 2>/dev/null)
		int fd_null = open("/dev/null", O_WRONLY);
		if (fd_null >= 0) {
			dup2(fd_null, STDERR_FILENO);
			close(fd_null);
		}

		// Executem 'tar' directament sense shell. 
		// Els arguments van un a un i SEMPRE han d'acabar amb NULL.
		execlp("tar", "tar", "-czf", fitxer_tar, "-C", ruta_real_carpeta, ".", NULL);

		// Si execlp() funciona, el procés fill es transforma en tar i no arriba mai aquí.
		// Si arriba aquí, vol dir que l'execució ha fallat (ex: no està instal·lat tar).
		exit(EXIT_FAILURE);
	}
	else {
		// PROCÉS PARE: El teu servidor principal
		int status;
		// Ens posem en pausa només per a aquest fill concret fins que acabi de comprimir
		waitpid(pid, &status, 0);

		// Comprovem si el procés 'tar' ha acabat amb èxit
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			printf("[ERROR] Ha fallat la compressió de la carpeta: %s\n", ruta_real_carpeta);
			long long error = -1;
			write(client->socket, &error, sizeof(long long));
			unlink(fitxer_tar); // Netejem per si de cas
			return -1;
		}
	}
	// --- FI DEL SISTEMA SEGUR ---

	// 3. Obrim i enviem el fitxer (Això es manté igual que ho tenies)
	int fd = open(fitxer_tar, O_RDONLY);
	if (fd >= 0 && fstat(fd, &st) == 0) {
		long long mida_t = (long long)st.st_size;
		write(client->socket, &mida_t, sizeof(long long));

		char buf[LEN_PAQUET];
		ssize_t n;
		while ((n = read(fd, buf, sizeof(buf))) > 0) {
			write(client->socket, buf, n);
		}
		close(fd);
		printf("[INFO] Carpeta comprimida i enviada correctament.\n");
	}
	else {
		long long error = -1;
		write(client->socket, &error, sizeof(long long));
	}

	// 4. Neteja el disc
	unlink(fitxer_tar);
	return 0;
}

void op_cd(t_client* client) {
	char nou_dir[LEN_PATH];
	char ruta_desti_provisional[PATH_MAX];
	char ruta_absoluta_final[PATH_MAX];
	char ruta_arrel_absoluta[PATH_MAX];
	int resposta = NO_VALID;

	// 1. Llegim el directori sol·licitat pel client (netejant el buffer primer)
	memset(nou_dir, 0, LEN_PATH);
	if (read(client->socket, nou_dir, LEN_PATH) <= 0) {
		return; // Error de lectura o client desconnectat
	}

	// 2. Construïm la ruta provisional combinant l'arrel FTP, el path actual i el nou
	construir_ruta_real(client, nou_dir, ruta_desti_provisional);

	// 3. Obtenim la ruta absoluta real de la carpeta arrel del servidor
	if (realpath(FTP_ROOT, ruta_arrel_absoluta) == NULL) {
		perror("[ERROR CRÍTIC] No s'ha pogut resoldre l'arrel FTP (existeix la carpeta?)");
		write(client->socket, &resposta, sizeof(int));
		return;
	}

	// 4. Resolem la ruta destí per eliminar ".." o "." i validem que existeixi
	// realpath retornarà NULL si la carpeta que ha demanat el client no existeix
	if (realpath(ruta_desti_provisional, ruta_absoluta_final) != NULL) {

		// 5. Comprovem que la ruta final estigui DINS de la carpeta arrel del servidor
		if (strncmp(ruta_absoluta_final, ruta_arrel_absoluta, strlen(ruta_arrel_absoluta)) == 0) {

			// Extraiem la part relativa a l'arrel (el nou path virtual del client)
			const char* p_relatiu = ruta_absoluta_final + strlen(ruta_arrel_absoluta);
			const char* nou_path = (strlen(p_relatiu) == 0) ? "/" : p_relatiu;

			// 6. Actualitzem el path del client de forma segura (evitant buffer overflow)
			strncpy(client->path_actual, nou_path, LEN_PATH - 1);
			client->path_actual[LEN_PATH - 1] = '\0';

			resposta = VALID;
			printf("[OK] CD a: %s (Path virtual: %s)\n", ruta_absoluta_final, client->path_actual);
		}
		else {
			printf("[ALERTA] Intent de fuga detectat cap a: %s\n", ruta_absoluta_final);
		}
	}
	else {
		printf("[INFO] Intent de CD a un directori inexistent: %s\n", ruta_desti_provisional);
	}

	// 7. Enviem la resposta final al client (1 = VALID, 0 = NO_VALID)
	write(client->socket, &resposta, sizeof(int));
}

int op_get(t_client* client) {
	char nom_fitxer[LEN_BUFFER];
	char ruta_real[PATH_MAX];
	char buffer[LEN_PAQUET];
	struct stat st;

	memset(nom_fitxer, 0, LEN_BUFFER);
	if (read(client->socket, nom_fitxer, LEN_BUFFER) <= 0) return -1;

	construir_ruta_real(client, nom_fitxer, ruta_real);
	int fd = open(ruta_real, O_RDONLY);

	if (fd < 0) {
		long long error = -1; // Enviem error en 8 bytes
		write(client->socket, &error, sizeof(long long));
		return -2;
	}

	fstat(fd, &st);
	long long mida_gran = (long long)st.st_size;
	write(client->socket, &mida_gran, sizeof(long long));

	ssize_t n;
	while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
		write(client->socket, buffer, n);
	}
	close(fd);
	printf("[INFO] Fitxer enviat correctament: %s\n", ruta_real);
	return 0;
}