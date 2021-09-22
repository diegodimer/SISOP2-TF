.PHONY : help build-server build-client clean build-all

help :  ## Show this help.
	@sed -ne '/@sed/!s/## //p' $(MAKEFILE_LIST)

build-server: ## Build server object file
	g++ -c src/Server.cpp -I . -o bin/Server.o

build-client: ## Build client object file
	g++ -c src/Client.cpp -I . -o bin/Client.o

build-client-main: ## Build main_client object file
	g++ -c src/main_client.cpp -I . -o bin/main_client.o

build-client-app: ## Link client and main client objects in app_client app
	g++ bin/Client.o bin/main_client.o -o bin/app_client -lpthread

clean: ## Clean bin folder
	rm -rf ./bin/*

build-all: ## Build all
	@echo "Building client"
	make build-client
	make build-client-main
	make build-client-app




