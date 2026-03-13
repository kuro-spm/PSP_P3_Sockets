# --- Configuració ---
CXX = g++
CXXFLAGS = -Wall -I. -pthread 
BIN_DIR = bin_linux

# --- Executables ---
CLASS_SERVER  = $(BIN_DIR)/class_server
CLIENT_LINUX  = $(BIN_DIR)/client_linux

# --- Fonts (Ajustades a la teva estructura real) ---
CLASS_SERVER_SRC = ClassServer/main.cpp ClassServer/ConnexioClient.cpp ClassServer/ServerCpp.cpp
CLIENT_LINUX_SRC = ClientLinux/client_linux.cpp

# --- Regles Principals ---

all: setup $(CLASS_SERVER) $(CLIENT_LINUX)

# Crea la carpeta de binaris i la carpeta root si no existeixen
setup:
	mkdir -p $(BIN_DIR)
	mkdir -p ftp_root

# Compilació del Servidor
$(CLASS_SERVER): $(CLASS_SERVER_SRC)
	$(CXX) $(CXXFLAGS) $(CLASS_SERVER_SRC) -o $(CLASS_SERVER)

# Compilació del Client
$(CLIENT_LINUX): $(CLIENT_LINUX_SRC)
	$(CXX) $(CXXFLAGS) $(CLIENT_LINUX_SRC) -o $(CLIENT_LINUX)

clean:
	rm -rf $(BIN_DIR)

# --- Comandes d'execució ---

run_server: all
	./$(CLASS_SERVER)

run_client: all
	./$(CLIENT_LINUX)
