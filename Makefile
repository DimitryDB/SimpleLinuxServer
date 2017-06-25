all: DB_Server DB_Client 
DB_Server:
	gcc DB_Server.c -D_REENTERANT -o DB_Server
DB_Client: 
	gcc DB_Client.c -D_REENTERANT -o DB_Client
