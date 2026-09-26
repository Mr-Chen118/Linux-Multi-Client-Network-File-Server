#include<errno.h>
#include"net_io.h"
#include<sys/socket.h>

int send_all(int fd, const void* buf, size_t len){
    size_t sent = 0;
    while(sent < len){
        ssize_t n = send(fd,(const char*)buf+sent,len-sent,0);
        if(n < 0){
            if(errno==EINTR){
                continue;
            }else{
                return -1;
            }
        }else if(n == 0){
            return -1;
        }
        sent+=(size_t)n;
    }
    return 0;
}

int recv_all(int fd, void* buf, size_t len){
    size_t receved=0;
    while(receved < len){
        ssize_t n=recv(fd,(char*)buf+receved,len-receved,0);
        if(n<0){
            if(errno==EINTR){
                continue;
            }
        return -1;
        }
        if(n==0){
            return -1;
        }
        receved+=(size_t)n;
    }
    return 0;
}

ssize_t recv_line(int fd, char *buf, size_t size){
    size_t used=0;
    if(size<2){
        return -1;
    }
    while(used+1<size){
        char ch;
        ssize_t n=recv(fd,&ch,1,0);
        if(n<0){
            if(errno==EINTR){
                continue;
            }
        return -1;
        }
        if(n==0){
            return -1;
        }
        if(ch=='\n'){
            buf[used]='\0';
            return used;
        }
        buf[used]=ch;
        used++;
    }
    buf[used]='\0';
    return -1;
}