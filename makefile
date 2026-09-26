CC ?= cc
CFLAGS ?= -Wall -Wextra -g
LDLIBS ?= -pthread

.PHONY: all clean

all: TCP_Server TCP_Client

TCP_Server: TCP_Server.c common.h net_io.c net_io.h
	$(CC) $(CFLAGS) -o $@ TCP_Server.c net_io.c $(LDLIBS)

TCP_Client: TCP_Client.c common.h net_io.c net_io.h
	$(CC) $(CFLAGS) -o $@ TCP_Client.c net_io.c $(LDLIBS)

clean:
	rm -f TCP_Server TCP_Client
