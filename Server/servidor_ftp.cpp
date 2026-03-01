#include "servidor_ftp.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

// Fem referència al semàfor global definit al main.cpp
extern pthread_mutex_t semafor_t_clients;

void inicialitzar_taula_clients(ControlClient* llista, int mida) {
	for (int i = 0; i < mida; i++) {
		llista[i].esta_ocupat = 0;
	}
}

int buscar_posicio_lliure(ControlClient* llista, int mida) {
	for (int i = 0; i < mida; i++) {
		if (llista[i].esta_ocupat == 0) return i;
	}
	return -1;
}

/// <summary>
/// Funció de neteja: Tanca el socket del client i allibera la ranura de la taula de canals.
/// </summary>
/// <param name="client_ptr"></param>
/// <returns></returns>
void* finalitzar_connexio_client(ControlClient* client_ptr) {
	printf("[INFO] Tancant socket %d\n", client_ptr->socket_cli);
	close(client_ptr->socket_cli);

	pthread_mutex_lock(&semafor_t_clients);
	client_ptr->esta_ocupat = 0;
	pthread_mutex_unlock(&semafor_t_clients);

	return NULL;
}



void* fil_gestio_client(void* argument_client) {
	ControlClient* client = (ControlClient*)argument_client;
	MissatgeHeader header;
	int resposta_validacio = 0; // 0: KO, 1: OK
	int resultat_login = 0; // 0: KO, 1: OK

	if (read(client->socket_cli, &header, sizeof(MissatgeHeader)) > 0) {
		// 1. Validar usuari/pass amb xifrar_password
		if (read(client->socket_cli, &header, sizeof(MissatgeHeader)) > 0) {
			// Validem les credencials que venen al header i que la operació sigui vàlida:
			if (validar_usuari(header.usuari, header.contrasenya) != 0 &&
				header.operacio >= 1 && header.operacio <= 3) {

				resposta_validacio = 1; // Tot és correcte
			}

		}
		// Informem al client del resultat de la validació
		write(client->socket_cli, &resposta_validacio, sizeof(int));

		if (resposta_validacio == 1) {
			// 2. Switch segons header.operacio:
			switch (header.operacio) {
				/*	OP_LS = 1,
					OP_CD = 2,
					OP_DOWNLOAD = 3 */
			case OP_LS: dir_servidor(client); break;
			case OP_CD: cd_path(client); break;
			case OP_DOWNLOAD: download_file(client); break;

			}

		}
	}
	return finalitzar_connexio_client(client);
}

/// <summary>
/// Valida si l'usuari i la contrasenya es troben al fitxer usuaris.txt. 
/// </summary>
/// <param name="usr"></param>
/// <param name="pwd"></param>
/// <returns>0 si no està registrat. 1 si està registrat.</returns>
int validar_usuari(char* usr, char* pwd) {
	FILE* fitxer = fopen("usuaris.txt", "r");
	if (fitxer == NULL) return 0; // Error o fitxer no existeix

	char linia[100];
	char u_fitxer[20], p_fitxer[100];
	int trobat = 0;

	// Xifrem la contrasenya rebuda per comparar-la amb la del fitxer
	xifrar_password(pwd);

	while (fgets(linia, sizeof(linia), fitxer)) {
		// Separem usuari i password de la línia (format usuari:pass)
		sscanf(linia, "%[^:]:%s", u_fitxer, p_fitxer);
					
		if (strcmp(usr, u_fitxer) == 0 && strcmp(pwd, p_fitxer) == 0) {
			trobat = 1;
			break;
		}
	}
	fclose(fitxer);
	return trobat;
}



//=========================Funcions del servidor=========================


/// <summary>
/// Llistarà els arxius del directori actual del client.
/// El resultat del ls/dir s'ha de convertir en un arxiu de text temporal amb un identificador del client, el qual s'enviarà al client.
/// </summary>
/// <param name="socket_client"></param>
void dir_servidor(ControlClient* client)
{

}

/// <summary>
/// Usa un path absolut o relatiu per posicionar-se en una carpeta determinada
/// </summary>
/// <param name="socket_client"></param>
/// <param name="path">Path al que es vol posicionar</param>
void cd_path(ControlClient* client)
{
	char nou_path[256];
	read(client->socket_cli, nou_path, sizeof(nou_path));

	strcpy(client->path_actual, nou_path);

	printf("[INFO:CD] El client %d ha canviat a: %s\n", client->socket_cli, client->path_actual);

}

/// <summary>
/// Descarrega un fitxer a la màquina del client amb el mateix nom que té en el servidor.
/// </summary>
/// <param name="socket_client"></param>
/// <param name="nom_fitxer"></param>
void download_file(ControlClient* client)
{

}

/// <summary>
/// Descarrega tota la carpeta nom_directori amb tot el seu contingut recursivament. 
/// Es construeix un arbre en el client amb els mateixos arxius que en el servidor. 
/// L'arrel de la estructura en el client serà nom_directori.
/// 
/// </summary>
/// <param name="socket_client"></param>
/// <param name="nom_directori"></param>
void rget_directory(ControlClient* client)
{
	//Farem servir TARGZ de la carpeta. El client ho desempaquetarà i crearà l'arbre automàticament.
}
