#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<pthread.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/select.h>
#include<arpa/inet.h> 
#include"common.h"

void *handle_client(void *arg){
    int client_fd = *(int*)arg;
    char *read_buf=NULL;
    char *write_buf=NULL;
    ssize_t count=0,send_count=0;
    read_buf = malloc(sizeof(char)*BUF_SIZE);
    write_buf = malloc(sizeof(char)*BUF_SIZE);
    if(!read_buf){
        perror("malloc read_buf");
        close(client_fd);
        return NULL;
    }
    if(!write_buf){
        perror("malloc write_buf");
        close(client_fd);
        return NULL;
    }
    while(1){
        count = recv(client_fd,read_buf,BUF_SIZE,0);
        if(count<0){
            perror("recv");
        }else if(count==0){
            printf("客户端%d已关闭连接\n",client_fd);
            break;
        }
        read_buf[count]='\0';
        printf("客户端%d:%s\n",client_fd,read_buf);

        strcpy(write_buf,"服务器已收到\n");
        int total=0 , len = strlen(write_buf);
        while(total<len){
            send_count = send(client_fd,write_buf+total,len-total,0);
            if(send_count<0){
            perror("send");
            break;
            }
            total+=send_count;
        }
    }
    close(client_fd);
    free(read_buf);
    free(write_buf);
    return NULL;
}

int main(int argc, char const *argv[])
{
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd<0){
        perror("socket");
        return EXIT_FAILURE;
    }
    struct sockaddr_in serv_addr;
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(6666);
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);

    if (bind(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr))){
        perror("bind");
        return EXIT_FAILURE;
    }

    if(listen(sockfd,100)){
        perror("listen");
        return EXIT_FAILURE;
    }

    struct sockaddr_in client_addr;
    memset(&client_addr,0,sizeof(client_addr));
    int client_addr_len = sizeof(client_addr);
    while(1){
        int connfd = accept(sockfd,(struct sockaddr *)&client_addr,&client_addr_len);
        int *p=malloc(sizeof(int));
        *p=connfd;
        pthread_t pid;
        if((*p)<0){
            perror("accept");
            continue;
        }
        printf("与客户端%s %d 建立连接%d\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port),connfd);
        if(pthread_create(&pid,NULL,handle_client,(void*)p)){
            perror("pthread_create");
            pid=-1;
            close(connfd);
            free(p);
        }
        pthread_detach(pid);
        client_addr_len = sizeof(client_addr);
    }
    
    printf("释放资源\n");
    close(sockfd);
}
