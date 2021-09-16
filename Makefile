.PHONY : help build-server build-client clean build-all

help :  ## Show this help.
	@sed -ne '/@sed/!s/## //p' $(MAKEFILE_LIST)

build-server: ## Build server application
	g++ src/Server.cpp -o ./bin/Server -lpthread -g

build-client: ## Build client application
	g++ src/Client.cpp -o ./bin/Client -lpthread -g

clean: ## Clean bin folder
	rm -rf ./bin/*

build-all: ## Build all
	make build-client
	make build-server




