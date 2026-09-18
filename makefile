TCP_Server:TCP_Server.c common.h
	$(CC) -pthread -Wall -g -o $@ TCP_Server.c

TCP_Client:TCP_Client.c common.h
	$(CC) -pthread -Wall -g -o $@ TCP_Client.c