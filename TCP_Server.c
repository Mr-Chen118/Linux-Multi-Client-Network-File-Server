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
#include"net_io.h"

int sockfd_arr[MAX_CLIENT];
int online_count=0;
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;


void *handle_client(void *arg);
int sign_in_client(void *arg);
void *remove_client(void *arg);

int sign_in_client(void *arg){
    pthread_mutex_lock(&mutex);
    int client_fd = *(int*)arg;
    free(arg);
    if(online_count>=MAX_CLIENT){
        printf("服务器已满，客户端%d注册失败\n",client_fd);
        close(client_fd);
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    for(int i=0;i<MAX_CLIENT;i++){
        if(sockfd_arr[i]<=0){
            sockfd_arr[i]=client_fd;
            online_count++;
            int *t_id=malloc(sizeof(int));
            *t_id=client_fd;
            pthread_t tid;
            if(pthread_create(&tid,NULL,handle_client,(void*)t_id)){
                perror("pthread_create");
                close(client_fd);
                sockfd_arr[i]=0;
                online_count--;
                pthread_mutex_unlock(&mutex);
                return -1;
            }
            pthread_detach(tid);
            pthread_mutex_unlock(&mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&mutex);
    return -1;
}
void *remove_client(void *arg){
    pthread_mutex_lock(&mutex);
    int client_fd = *(int*)arg;
    free(arg);
    for(int i=0;i<MAX_CLIENT;i++){
        if(sockfd_arr[i]==client_fd){
            sockfd_arr[i]=0;
            online_count--;
            break;
        }
    } 
    pthread_mutex_unlock(&mutex);
    printf("客户端%d已关闭连接\n",client_fd);
    return NULL;
}

void *send_broadcast(char *msg){
    pthread_mutex_lock(&mutex);
    int len = strlen(msg);
    for(int i=0;i<MAX_CLIENT;i++){
        if(sockfd_arr[i]>0 ){
            int total=0;
            while(total<len){
                int ret=send(sockfd_arr[i],msg+total,len-total,0);
                if(ret<=0){
                    perror("send");
                    shutdown(sockfd_arr[i],SHUT_RDWR);
                    break;
                }
                total+=ret;
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void *handle_client(void *arg){
    int client_fd = *(int*)arg;
    free(arg);
    char *read_buf=NULL;
    char *write_buf=NULL;
    ssize_t count=0;
    read_buf = malloc(sizeof(char)*BUF_SIZE);
    write_buf = malloc(sizeof(char)*BUF_SIZE);
    if(!read_buf){
        perror("malloc read_buf");
        close(client_fd);
        return NULL;
    }
    if(!write_buf){
        perror("malloc write_buf");
        free(read_buf);
        return NULL;
    }
    while(1){
        count = recv_line(client_fd,read_buf,BUF_SIZE);
        if(count==0){
            continue;
        }
        if(count<0){
            printf("客户端%d读取失败\n",client_fd);
            int *p_fd = malloc(sizeof(int));
            *p_fd = client_fd;
            remove_client(p_fd);
            close(client_fd);
            free(read_buf);
            free(write_buf);
            break;
        }
        
        char match[]="UPLOAD ";
        if(strncmp(read_buf,match,strlen(match))==0){
            char filename[256];
            long long file_size;
            int parsed = sscanf(read_buf,"UPLOAD %255s %lld",filename,&file_size);
            if(parsed!=2){
                printf("客户端%d上传命令格式错误\n",client_fd);
                continue;
            }
            if(file_size<0){
                printf("客户端%d上传命令文件大小不能为负数\n",client_fd);
                continue;
            }
            if(strchr(filename,'/')!=NULL){
                printf("客户端%d上传的文件名非法\n", client_fd);
                continue;
            }
            char file_path[512];
            int path_len = snprintf(file_path,sizeof(file_path),"server_files/%s",filename);
            if(path_len<0 || (size_t)path_len>=sizeof(file_path)){
                printf("客户端%d上传的文件名过长\n", client_fd);
                continue;
            }
            FILE *fp = fopen(file_path,"wb");
            if(!fp){
                perror("fopen");
                continue;
            }
            long long remaining=file_size;
            int upload_ok=1;
            while(remaining>0){
                size_t chunk=remaining<BUF_SIZE?remaining:BUF_SIZE;
                if(recv_all(client_fd,read_buf,chunk)!=0){
                    printf("客户端%d接受文件失败\n",client_fd);
                    upload_ok=0;
                    break;
                }
                if(fwrite(read_buf,1,chunk,fp)!=chunk){
                    printf("客户端%d写入文件失败\n",client_fd);
                    upload_ok=0;
                    break;
                }
                remaining-=chunk;
            }
            fclose(fp);
            if(upload_ok&&remaining==0){
                send_all(client_fd,"UPLOAD_OK\n",10);
                printf("客户端%d上传文件完成 %s\n\n",client_fd,file_path);
            }

            printf("文件名: %s, 大小: %lld\n",filename,file_size);
            continue;
        }


        printf("客户端%d:%s\n",client_fd,read_buf);

        snprintf(write_buf,BUF_SIZE,"客户端%d:%s\n",client_fd,read_buf);
        send_broadcast(write_buf);
    }
    return NULL;
}

int main(void)
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
    socklen_t client_addr_len = sizeof(client_addr);
    while(1){
        client_addr_len = sizeof(client_addr);
        int connfd = accept(sockfd,(struct sockaddr *)&client_addr,&client_addr_len);
        int *p=malloc(sizeof(int));
        *p=connfd;
        if((*p)<0){
            perror("accept");
            free(p);
            continue;
        }
        if(sign_in_client(p)<0){
            printf("客户端%d注册失败\n",connfd);
        }else{
            printf("与客户端%s %d 建立连接%d\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port),connfd);
        }
    
    }
    printf("服务器已关闭\n");
    printf("释放资源\n");
    close(sockfd);
}
