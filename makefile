# --- Configuración ---
CXX = g++
# -pthread es vital para sockets y multihilo en Linux
CXXFLAGS = -Wall -I. -pthread 
BIN_DIR = bin_linux

# --- Ejecutables ---
SERVER_PLAIN = $(BIN_DIR)/server_plain
CLASS_SERVER  = $(BIN_DIR)/class_server
CLIENT_LINUX  = $(BIN_DIR)/client_linux

# --- Fuentes ---
# Server: main.cpp y servidor_ftp.cpp
SERVER_SRC = Server/main.cpp Server/servidor_ftp.cpp

# ClassServer: main.cpp, ConnexioClient.cpp y ServerCpp.cpp
CLASS_SERVER_SRC = ClassServer/main.cpp ClassServer/ConnexioClient.cpp ClassServer/ServerCpp.cpp

# ClientLinux: client_linux.cpp
CLIENT_LINUX_SRC = ClientLinux/client_linux.cpp

# --- Reglas ---

all: setup $(SERVER_PLAIN) $(CLASS_SERVER) $(CLIENT_LINUX)

setup:
	@mkdir -p $(BIN_DIR)

$(SERVER_PLAIN): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) $(SERVER_SRC) -o $(SERVER_PLAIN)

$(CLASS_SERVER): $(CLASS_SERVER_SRC)
	$(CXX) $(CXXFLAGS) $(CLASS_SERVER_SRC) -o $(CLASS_SERVER)

$(CLIENT_LINUX): $(CLIENT_LINUX_SRC)
	$(CXX) $(CXXFLAGS) $(CLIENT_LINUX_SRC) -o $(CLIENT_LINUX)

clean:
	rm -rf $(BIN_DIR)

# --- Ejecución en terminales separadas ---

# Lanza el servidor básico
run_server: $(SERVER_PLAIN)
	x-terminal-emulator -e "bash -c './$(SERVER_PLAIN); exec bash'" &

# Lanza el servidor con clases
run_class_server: $(CLASS_SERVER)
	x-terminal-emulator -e "bash -c './$(CLASS_SERVER); exec bash'" &

# Lanza el cliente Linux
run_client: $(CLIENT_LINUX)
	x-terminal-emulator -e "bash -c './$(CLIENT_LINUX); exec bash'" &

# Test rápido: lanza ClassServer y luego el Cliente
test: all
	x-terminal-emulator -e "bash -c './$(CLASS_SERVER); exec bash'" &
	@sleep 1
	x-terminal-emulator -e "bash -c './$(CLIENT_LINUX); exec bash'" &