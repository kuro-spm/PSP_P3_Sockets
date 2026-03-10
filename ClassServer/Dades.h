#ifndef DADES_H 
#define DADES_H

#define PORT_SERVEI 10235
#define SOCKET_ATURAT -1
#define VALID 1
#define NO_VALID 0
#define LEN_BUFFER 256
#define LEN_PAQUET 1024
#define LEN_USUARI 30
#define LEN_CONTRASENYA 30

// Definició de codis d'operació
enum CodisOperacio {
	OP_LS = 1,
	OP_CD = 2,
	OP_DOWNLOAD = 3,
	OP_REGISTRE = 4,
	OP_SORTIR = 5
};

// Estructura del protocold
typedef struct {
	int operacio;
	int versio;
	char usuari[LEN_USUARI];
	char contrasenya[LEN_CONTRASENYA];
	int len;
} ConnectionHeader;

typedef struct {
	char* info;
	int len;
} OperationHeader;



#endif // DADES_H