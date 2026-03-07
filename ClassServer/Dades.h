#ifndef DADES_H 
#define DADES_H

#define PORT_SERVEI 10235
#define SOCKET_ATURAT -1
#define VALID 1
#define NO_VALID 0

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
    char usuari[20];
    char contrasenya[20];
    int len;
} MissatgeHeader;

#endif // DADES_H