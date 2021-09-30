.PHONY : help build-server build-client clean build-all

help :  ## Show this help.
	@sed -ne '/@sed/!s/## //p' $(MAKEFILE_LIST)

build-server: ## Build server object file
	g++ -c src/Server.cpp -I . -o bin/Server.o -g

build-client: ## Build client object file
	g++ -c src/Client.cpp -I . -o bin/Client.o -g

build-socket: ## Build Socket object file
	g++ -c src/SocketClient.cpp -I . -o bin/SocketClient.o -g

build-message: ## Build Message object file
	g++ -c src/Message.cpp -I . -o bin/Message.o -g

build-client-main: ## Build main_client object file
	g++ -c src/main_client.cpp -I . -o bin/main_client.o -g

build-server-main: ## Build main_server object file
	g++ -c src/main_server.cpp -I . -o bin/main_server.o -g

build-client-app: ## Link client and main client objects in app_client app
	g++ bin/Client.o bin/main_client.o bin/SocketClient.o bin/Message.o -o bin/app_client -lpthread -g

build-socket-app: ## Link client and main client objects in app_client app
	g++ bin/SocketClient.o bin/Message.o -o bin/socket -lpthread -g

clean: ## Clean bin folder
	rm -rf ./bin/*

build-all: ## Build all (client related)
	@echo "Building client"
	make build-socket
	make build-message
	make build-client
	make build-client-main
	make build-client-app

build-socket-test: ## testing purposes: build socket client + socket
	g++ -c src/SocketTest.cpp -I . -o bin/socket_client_test.o
	g++ bin/Message.o bin/socket_client_test.o -o bin/socket_client_test -lpthread

build-server-test:
	make build-message
	make build-server-main
	g++ bin/Message.o bin/main_server.o -o bin/main_server.exe -lpthread




