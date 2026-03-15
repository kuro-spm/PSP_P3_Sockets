#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <fcntl.h>    
#include <sys/stat.h> 
#include <stdlib.h>   
#include <io.h>             // Per a open(), read(), write() en Windows
#include "../ClassServer/Dades.h"

// --- LLIBRERIES ESPECÍFIQUES DE WINDOWS ---
#include <winsock2.h>       // La base dels sockets en Windows
#include <ws2tcpip.h>       // Per a inet_pton i estructures modernes
#include <direct.h>         // Per a _mkdir() i _chdir()

// Windows: Enllaçar la llibreria de sistema ws2_32
#pragma comment(lib, "ws2_32.lib")

#define CARPETA_DESCARREGUES "./ftpDownloads"
#define IP_SERVER "192.168.68.109"

// Demana l'usuari i la contrasenya
void demanar_usuari_pwd(ConnectionHeader* h) {
	printf("--- AUTENTICACIÓ (WINDOWS CLIENT) ---\n");
	printf("Usuari: ");
	scanf("%s", h->usuari);
	printf("Contrasenya: ");
	scanf("%s", h->contrasenya);
	h->versio = 1;
	strncpy(h->path_actual, "/", LEN_PATH - 1);
}

// Menú d'operacions
int demanar_operacio() {
	int opcio;
	printf("\nSELECCIONA UNA OPERACIÓ:\n");
	printf("%d. Llistar fitxers (ls)\n", OP_DIR);
	printf("%d. Canviar directori (cd)\n", OP_CD);
	printf("%d. Descarregar fitxer (get)\n", OP_GET);
	printf("%d. Descarregar carpeta (rget)\n", OP_RGET);
	printf("%d. Sortir\n", OP_SORTIR);
	printf("Opcio: ");

	if (scanf("%d", &opcio) != 1) {
		while (getchar() != '\n');
		return -1;
	}
	return opcio;
}

char path_local[LEN_PATH] = "/";

int main() {
	struct sockaddr_in server_addr;
	ConnectionHeader header;
	char buffer_rebut[LEN_PAQUET];
	bool sistema_actiu = true;

	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	// --- 1. INICIALITZACIÓ WINSOCK (OBLIGATORI A WINDOWS) ---
	WSADATA wsaData;
	// Linux: No existeix
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("Error a WSAStartup\n");
		return 1;
	}

	// --- 2. GESTIÓ DE CARPETES ---
	// Linux: system("mkdir -p ...");
	// Windows: _mkdir (no accepta el flag -p directament)
	_mkdir(CARPETA_DESCARREGUES);

	// Linux: chdir()
	// Windows: _chdir()
	if (_chdir(CARPETA_DESCARREGUES) != 0) {
		perror("[ERROR] No s'ha pogut accedir al directori");
		WSACleanup();
		return 1;
	}

	printf("[INFO] Client Windows a punt a: %s\n", CARPETA_DESCARREGUES);
	demanar_usuari_pwd(&header);

	while (sistema_actiu) {
		int op = demanar_operacio();
		if (op == OP_SORTIR) {
			sistema_actiu = false;
			break;
		}

		// --- INICI DE LA TRANSACCIÓ ---
		// Linux: int sock
		// Windows: SOCKET sock
		SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(PORT_SERVEI);
		server_addr.sin_addr.s_addr = inet_addr(IP_SERVER);

		if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
			perror("Error de connexió");
			continue;
		}

		header.operacio = op;
		strncpy(header.path_actual, path_local, LEN_PATH - 1);
		header.path_actual[LEN_PATH - 1] = '\0';

		// --- ENVIAR HEADER ---
		// Linux: write(sock, &header, sizeof(header));
		// Windows: send(sock, ...)
		send(sock, (const char*)&header, sizeof(ConnectionHeader), 0);

		// --- REBRE VALIDACIÓ ---
		int validacio;
		// Linux: if (read(sock, &validacio, sizeof(int)) <= 0...
		// Windows: if (recv(sock, (char*)&validacio...
		if (recv(sock, (char*)&validacio, sizeof(int), 0) <= 0 || validacio != VALID) {
			printf("Error d'autenticació o servidor tancat.\n");
			closesocket(sock); // Linux: close(sock);
			continue;
		}

		switch (op) {
		case OP_DIR: {
			long long mida;
			// Rebem la mida (8 bytes)
			if (recv(sock, (char*)&mida, sizeof(mida), 0) > 0) {
				long long rebut = 0;
				printf("\n--- Contingut de %s (%lld bytes) ---\n", path_local, mida);

				while (rebut < mida) {
					// Netegem el buffer abans de rebre
					memset(buffer_rebut, 0, sizeof(buffer_rebut));
					int n = recv(sock, buffer_rebut, sizeof(buffer_rebut) - 1, 0);
					if (n <= 0) break;
					buffer_rebut[n] = '\0';
					printf("%s", buffer_rebut);
					rebut += n;
				}
				printf("\n"); // Un salt de línia extra per seguretat
			}
			break;
		}

		case OP_CD: {
			char nou_dir[LEN_PATH];
			memset(nou_dir, 0, LEN_PATH);
			printf("Destí: ");
			scanf("%s", nou_dir);

			// Enviem el bloc fix de 256 bytes per sincronitzar el servidor
			send(sock, nou_dir, LEN_PATH, 0);

			int ok;
			recv(sock, (char*)&ok, sizeof(int), 0);
			if (ok == VALID) {
				// Actualitzem la ruta visual local
				if (strcmp(nou_dir, "..") == 0) {
					char* barra = strrchr(path_local, '/');
					if (barra != path_local) *barra = '\0'; else strcpy(path_local, "/");
				}
				else {
					if (strcmp(path_local, "/") != 0) strcat(path_local, "/");
					strcat(path_local, nou_dir);
				}
			}
			else printf("Error: Accés denegat o directori inexistent.\n");
			break;
		}

		case OP_GET: {
			char fitxer[LEN_BUFFER];
			memset(fitxer, 0, LEN_BUFFER);
			printf("Fitxer: "); scanf("%s", fitxer);
			send(sock, fitxer, LEN_BUFFER, 0);

			long long mida_f;
			if (recv(sock, (char*)&mida_f, sizeof(long long), 0) > 0 && mida_f >= 0) {
				int fd = _open(fitxer, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, 0666);
				long long r = 0;
				while (r < mida_f) {
					int n = recv(sock, buffer_rebut, sizeof(buffer_rebut), 0);
					if (n <= 0) break;
					_write(fd, buffer_rebut, n);
					r += n;
				}
				_close(fd);
				printf("[OK] Descarregat.\n");
			}
			else printf("Error: El fitxer no existeix.\n");
			break;
		}
		case OP_RGET: {
			char nom_dir[LEN_BUFFER];
			memset(nom_dir, 0, LEN_BUFFER);
			printf("Carpeta a descarregar: ");
			scanf("%s", nom_dir);
			send(sock, nom_dir, LEN_BUFFER, 0);

			long long mida_tar;
			if (recv(sock, (char*)&mida_tar, sizeof(long long), 0) > 0 && mida_tar > 0) {
				int fd_temp = _open("rebut.tar.gz", _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, 0666);
				long long total_rebut = 0;
				while (total_rebut < mida_tar) {
					int n = recv(sock, buffer_rebut, sizeof(buffer_rebut), 0);
					if (n <= 0) break;
					_write(fd_temp, buffer_rebut, n);
					total_rebut += n;
				}
				_close(fd_temp);

				_mkdir(nom_dir);
				char cmd[512];
				sprintf(cmd, "tar -xzf rebut.tar.gz -C %s", nom_dir);
				system(cmd);
				_unlink("rebut.tar.gz");

				char ruta_absoluta[1024];
				if (_getcwd(ruta_absoluta, sizeof(ruta_absoluta)) != NULL) {
					printf("[OK] Carpeta '%s' descomprimida correctament a:\n", nom_dir);
					printf("     %s\\%s\n", ruta_absoluta, nom_dir);
				}
				else {
					printf("[OK] Carpeta '%s' descarregada.\n", nom_dir);
				}
			}
			else {
				printf("[!] Error: No s'ha pogut rebre la carpeta del servidor.\n");
			}
			break;
		}
		}

		// --- FINAL DE LA TRANSACCIÓ ---
		// Linux: close(sock);
		closesocket(sock);
	}

	// --- TANCAR WINSOCK ---
	WSACleanup();
	return 0;
}