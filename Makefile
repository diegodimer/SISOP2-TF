.PHONY : help build-server build-client clean build-all

help :  ## Show this help.
	@sed -ne '/@sed/!s/## //p' $(MAKEFILE_LIST)

build-server: ## Build server object file
	g++ -c src/Server.cpp -I . -o bin/Server.o

build-client: ## Build client object file
	g++ -c src/Client.cpp -I . -o bin/Client.o

build-socket: ## Build Socket object file
	g++ -c src/Socket.cpp -I . -o bin/Socket.o -g

build-message: ## Build Message object file
	g++ -c src/Message.cpp -I . -o bin/Message.o

build-client-main: ## Build main_client object file
	g++ -c src/main_client.cpp -I . -o bin/main_client.o

build-client-app: ## Link client and main client objects in app_client app
	g++ bin/Client.o bin/main_client.o -o bin/app_client -lpthread

build-socket-app: ## Link client and main client objects in app_client app
	g++ bin/Socket.o bin/Message.o -o bin/socket -lpthread -g

clean: ## Clean bin folder
	rm -rf ./bin/*

build-all: ## Build all
	@echo "Building client"
	make build-client
	make build-client-main
	make build-client-app

build-socket-test: ## testing purposes: build socket client + socket
	make build-message
	make build-socket
	make build-socket-app
	g++ -c src/SocketClient.cpp -I . -o bin/socket_client.o
	g++ bin/Message.o bin/socket_client.o -o bin/socket_client -lpthread




