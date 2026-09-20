CC ?= cc
CFLAGS ?= -Wall -Wextra -g
LDLIBS ?= -pthread

.PHONY: all clean

all: TCP_Server TCP_Client

TCP_Server: TCP_Server.c common.h
	$(CC) $(CFLAGS) -o $@ TCP_Server.c $(LDLIBS)

TCP_Client: TCP_Client.c common.h
	$(CC) $(CFLAGS) -o $@ TCP_Client.c $(LDLIBS)

clean:
	rm -f TCP_Server TCP_Client
