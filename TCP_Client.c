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

void *read_from_server(void *arg){
    int sockfd = *((int*)arg);
    char read_buf[BUF_SIZE];
    while(1){int n= recv(sockfd,read_buf,sizeof(read_buf)-1,0);
        read_buf[n]='\0';
        fputs(read_buf,stdout);
        fflush(stdout);
        if(n < 0){
            perror("recv");
            continue;
        }else if(n==0){
            printf("服务器已关闭连接\n");
            break;
        }
    }
    shutdown(sockfd,SHUT_RDWR);
    return NULL;
}

void *write_to_server(void *arg){
    int sockfd = *((int*)arg);
    char write_buf[BUF_SIZE];
    while(fgets(write_buf,sizeof(write_buf),stdin)!=NULL){
        if(send(sockfd,write_buf,strlen(write_buf),0)<=0){
            perror("send");
            break;
        }
    }
    return NULL;
}

int main(int argc, char const *argv[])
{
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd==-1){
        perror("socket");
        return -1;
    }

    
    struct sockaddr_in server_info,client_info;
    memset(&client_info,0,sizeof(client_info));
    memset(&server_info,0,sizeof(server_info));
    
    server_info.sin_family=AF_INET;
    inet_pton(AF_INET,"127.0.0.1",&server_info.sin_addr);
    server_info.sin_port=htons(6666);

    int ret_con =connect(sockfd,(struct sockaddr*)&server_info,sizeof(server_info));
    if(ret_con<0){
        perror("connect");
        close(sockfd);
        return -1;
    }
    printf("%s %d 连接成功\n",inet_ntoa(server_info.sin_addr),ntohs(server_info.sin_port));

    pthread_t pid1,pid2;
    pthread_create(&pid1,NULL,read_from_server,(void*)&sockfd);
    pthread_create(&pid2,NULL,write_to_server,(void*)&sockfd);
    pthread_join(pid1,NULL);
    pthread_join(pid2,NULL);

    printf("释放资源\n");
    close(sockfd);

    return 0;
}
