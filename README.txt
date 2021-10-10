Instruções para rodar o código:

Primeiro compilar o código via 
	make build-all

Depois brir o servidor: 
	make run-server-default (Abre o server no IP 127.0.0.1, porta 4002)

E abrir uma seleção dos clientes:
	make run-miku-default
	make run-miku2-default
	make run-oblige-default
	make run-noblesse-default

Cliente @miku, @miku2 seguem @oblige.
Cliente @oblige segue @miku.

Para rodar servidor ou clientes em outro IP ou porta diferente:

	./bin/main_server <port #>
	./bin/app_client <username> <IP> <port #>


NOTA: Existe um bug transiente em que, ao tentar conectar três sessões de um usuário, a última conexão do usuário feita com sucesso não consegue receber updates, mas a primeira consegue. Se isso acontecer, simplesmente reiniciar as aplicações.
