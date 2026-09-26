#ifndef NET_IO_H
#define NET_IO_H

#include<stdio.h>
#include<sys/types.h>

int send_all(int fd, const void* buf, size_t len);
int recv_all(int fd, void* buf, size_t len);
ssize_t recv_line(int fd, char *buf, size_t size);

#endif