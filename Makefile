make: client server new_server

client: client.c
	gcc client.c -o client

server: server.c
	gcc server.c -o server
	
new_server: new_server.c
	gcc new_server.c -o new_server