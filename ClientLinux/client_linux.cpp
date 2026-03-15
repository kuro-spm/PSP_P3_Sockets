#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>    
#include <sys/stat.h> 
#include <stdlib.h>   
#include "../ClassServer/Dades.h"

#define CARPETA_DESCARREGUES "./ftpDownloads"
//define IP_SERVER "127.0.0.1"
#define IP_SERVER "192.168.68.109"



// Demana l'usuari i la contrasenya i els guarda al header
void demanar_usuari_pwd(ConnectionHeader* h) {
	printf("--- AUTENTICACIÓ ---\n");
	printf("Usuari: ");
	scanf("%s", h->usuari);
	printf("Contrasenya: ");
	scanf("%s", h->contrasenya);
	// Inicialitzem la versió i el path per defecte al header
	h->versio = 1;
	strncpy(h->path_actual, "/", LEN_PATH - 1);
}

// Mostra el menú i retorna el codi de l'operació
int demanar_operacio() {
	int opcio;
	printf("\nSELECCIONA UNA OPERACIÓ:\n");
	printf("%d. Llistar fitxers (ls)\n", OP_DIR);
	printf("%d. Canviar directori (cd)\n", OP_CD);
	printf("%d. Descarregar fitxer (get)\n", OP_GET);
	printf("%d. Descarregar carpeta (rget)\n", OP_RGET);
	//printf("%d. Registrar nou usuari\n", OP_REGISTRE); 
	printf("%d. Sortir\n", OP_SORTIR);
	printf("Opcio: ");

	if (scanf("%d", &opcio) != 1) {
		//Futura implementació: Acceptar també les operacions com a text (ls, cd, get, rget, registre, sortir)
		while (getchar() != '\n');
		return -1;
	}
	return opcio;
}

// Memòria local del client
char path_local[LEN_PATH] = "/";

int main() {
	struct sockaddr_in server_addr;
	ConnectionHeader header;
	char buffer_rebut[LEN_PAQUET];
	bool sistema_actiu = true;

	// --- 0. CREACIÓ DE LA CARPETA ---
	char comanda_mkdir[128];
	snprintf(comanda_mkdir, sizeof(comanda_mkdir), "mkdir -p %s", CARPETA_DESCARREGUES);

	if (system(comanda_mkdir) != 0) {
		fprintf(stderr, "[ERROR] No s'ha pogut crear la carpeta de descàrregues %s\n", CARPETA_DESCARREGUES);
		return 1;
	}
	else {
		printf("[INFO] Carpeta de descàrregues a punt: %s\n", CARPETA_DESCARREGUES);
	}

	// Entrem a la carpeta per treballar-hi dins
	if (chdir(CARPETA_DESCARREGUES) != 0) {
		perror("[ERROR] No s'ha pogut accedir al directori de descàrregues");
		return 1;
	}

	//char cwd[1024];
	//if (getcwd(cwd, sizeof(cwd)) != NULL) {
	//	printf("[DEBUG] La ruta absoluta és: %s\n", cwd);
	//}
	//else {
	//	perror("getcwd() error");
	//}

	// Demanar credencials a l'usuari abans de començar el bucle principal
	demanar_usuari_pwd(&header);

	while (sistema_actiu) {
		int op = demanar_operacio();
		if (op == OP_SORTIR) {
			sistema_actiu = false;
			break;
		}

		// --- INICI DE LA TRANSACCIÓ ---
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(PORT_SERVEI);
		server_addr.sin_addr.s_addr = inet_addr(IP_SERVER);

		if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
			perror("Error de connexió");
			break;
		}

		// 1. Preparem el Header amb l'operació i el path actual
		header.operacio = op;
		strncpy(header.path_actual, path_local, LEN_PATH - 1);
		header.path_actual[LEN_PATH - 1] = '\0';

		// 2. Enviem tot el bloc de cop (Header + Path + Credencials)
		write(sock, &header, sizeof(ConnectionHeader));

		// 3. Rebre validació
		int validacio;
		if (read(sock, &validacio, sizeof(int)) <= 0 || validacio != VALID) {
			printf("Error d'autenticació o servidor tancat.\n");
			close(sock);
			continue;
		}

		// 4. Executar lògica segons l'operació
		switch (op) {
		case OP_DIR: {
			long long mida;
			if (read(sock, &mida, sizeof(mida)) > 0) {
				long long rebut = 0;
				printf("\n--- Contingut de %s (%ld bytes) ---\n", path_local, mida);
				while (rebut < mida) {
					int n = read(sock, buffer_rebut, sizeof(buffer_rebut) - 1);
					if (n <= 0) break;
					buffer_rebut[n] = '\0';
					printf("%s", buffer_rebut);
					rebut += n;
				}
			}
			break;
		}

		case OP_CD: {
			char nou_dir[LEN_BUFFER];
			printf("Directori destí: ");
			scanf("%s", nou_dir);
			write(sock, nou_dir, strlen(nou_dir) + 1);

			int ok;
			read(sock, &ok, sizeof(int));
			if (ok == VALID) {
				if (nou_dir[0] == '/') strcpy(path_local, nou_dir);
				else {
					if (strcmp(path_local, "/") != 0) strcat(path_local, "/");
					strcat(path_local, nou_dir);
				}
				printf("Directori actualitzat localment: %s\n", path_local);
			}
			else {
				printf("Error: El servidor no troba aquest directori.\n");
			}
			break;
		}

		case OP_GET: {
			char fitxer[LEN_BUFFER];
			printf("Fitxer a descarregar: ");
			scanf("%s", fitxer);
			write(sock, fitxer, strlen(fitxer) + 1);

			long mida_f;
			if (read(sock, &mida_f, sizeof(long)) > 0 && mida_f >= 0) {
				int fd = open(fitxer, O_WRONLY | O_CREAT | O_TRUNC, 0666);
				if (fd < 0) {
					perror("Error al crear el fitxer local");
					break;
				}

				long total_f = 0;
				while (total_f < mida_f) {
					int n = read(sock, buffer_rebut, sizeof(buffer_rebut));
					if (n <= 0) break;
					write(fd, buffer_rebut, n);
					total_f += n;
				}
				close(fd);

				char ruta_real[1024];
				if (getcwd(ruta_real, sizeof(ruta_real)) != NULL) {
					printf("[OK] Fitxer descarregat a: %s/%s\n", ruta_real, fitxer);
				}
				else {
					printf("[OK] Fitxer descarregat correctament.\n");
				}
			}
			else {
				printf("Error: El fitxer no existeix al servidor.\n");
			}
			break;
		}

		case OP_RGET: {
			char nom_dir[LEN_BUFFER];
			printf("Nom de la carpeta a descarregar: ");
			scanf("%s", nom_dir);
			write(sock, nom_dir, strlen(nom_dir) + 1);

			long mida_tar;
			if (read(sock, &mida_tar, sizeof(long)) > 0 && mida_tar > 0) {
				printf("[+] Rebent carpeta (%ld bytes)... ", mida_tar);

				int fd_temp = open("rebut.tar.gz", O_WRONLY | O_CREAT | O_TRUNC, 0666);
				long total_rebut = 0;
				while (total_rebut < mida_tar) {
					int n = read(sock, buffer_rebut, sizeof(buffer_rebut));
					if (n <= 0) break;
					write(fd_temp, buffer_rebut, n);
					total_rebut += n;
				}
				close(fd_temp);

				mkdir(nom_dir, 0777);
				char cmd[LEN_BUFFER + 64];
				snprintf(cmd, sizeof(cmd), "tar -xzf rebut.tar.gz -C %s 2>/dev/null", nom_dir);
				system(cmd);
				unlink("rebut.tar.gz");

				// --- NOU MISSATGE AMB RUTA ABSOLUTA ---
				char ruta_real[1024];
				if (getcwd(ruta_real, sizeof(ruta_real)) != NULL) {
					printf("OK (Guardat a: %s/%s)\n", ruta_real, nom_dir);
				}
				else {
					printf("OK\n");
				}
			}
			else {
				printf("[!] Error: La carpeta no existeix o està buida.\n");
			}
			break;
		}
		//case OP_REGISTRE: {
		//	ConnectionHeader nou_usuari;
		//	printf("--- REGISTRE DE NOU USUARI ---\n");
		//	printf("Nou Usuari: ");
		//	scanf("%s", nou_usuari.usuari);
		//	printf("Nova Contrasenya: ");
		//	scanf("%s", nou_usuari.contrasenya);

		//	// Fem servir el mateix header per enviar la petició
		//	nou_usuari.operacio = OP_REGISTRE;
		//	strncpy(nou_usuari.path_actual, path_local, LEN_PATH);

		//	write(sock, &nou_usuari, sizeof(ConnectionHeader));

		//	int estat_registre;
		//	if (read(sock, &estat_registre, sizeof(int)) > 0) {
		//		if (estat_registre == VALID) printf("[OK] Usuari creat correctament.\n");
		//		else printf("[ERROR] L'usuari ja existeix o error al servidor.\n");
		//	}
		//	break;
		//}
		}

		// --- FINAL DE LA TRANSACCIÓ ---
		close(sock);
	}
	return 0;
}
