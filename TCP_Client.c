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
#include<signal.h>
#include"common.h"
#include"net_io.h"
#include<sys/stat.h>

void *read_from_server(void *arg){
    int sockfd = *((int*)arg);
    char read_buf[BUF_SIZE];
    while(1){
        int n=recv_line(sockfd,read_buf,sizeof(read_buf));
        if(n < 0){
            printf("服务器已关闭连接\n");
            break;
        }else if(n==0){
            continue;
        }

        if(strncmp(read_buf,"FILE ",5)==0){
            char filename[256];
            long long file_size;
            if(sscanf(read_buf,"FILE %255s %lld",&filename,&file_size)!=2||file_size<0){
                printf("下载文件头格式错误\n");
                continue;
            }
            if(strchr(filename,'/')!=NULL){
                printf("下载文件名非法\n");
                continue;
            }
            char file_path[512];
            int path_len=snprintf(file_path,sizeof(file_path),"downloads/%s",filename);
            if(path_len<0||(size_t)path_len>=sizeof(file_path)){
                printf("下载文件名过长\n");
                continue;
            }
            FILE* fp = fopen(file_path,"wb");
            if(fp==NULL){
                perror("fopen");
                continue;
            }
            long long remaining=file_size;
            char file_buf[BUF_SIZE];
            int download_ok=1;
            while(remaining>0){
                size_t chunk=remaining>BUF_SIZE?(size_t)BUF_SIZE:(size_t)remaining;
                if(recv_all(sockfd,file_buf,chunk)!=0){
                    printf("下载文件失败\n");
                    download_ok=0;
                    break;
                }
                if(fwrite(file_buf,1,chunk,fp)!=chunk){
                    printf("保存下载文件失败\n");
                    download_ok=0;
                    break;
                }
                remaining-=chunk;
            }
            fclose(fp);
            if(download_ok && remaining==0){
                printf("下载文件完成: %s\n",file_path);
            }
        }

        fputs(read_buf,stdout);
        fputc('\n',stdout);
        fflush(stdout);
    }
    shutdown(sockfd,SHUT_RDWR);
    return NULL;
}

void *write_to_server(void *arg){
    int sockfd = *((int*)arg);
    char write_buf[BUF_SIZE];
    while(fgets(write_buf,sizeof(write_buf),stdin)!=NULL){
        if(strncmp(write_buf,"/upload ",8)==0){
            char file_path[256];
            int parsed=sscanf(write_buf,"/upload %255s",file_path);
            if(parsed!=1){
                perror("sscanf");
                continue;
            }
            struct stat st;
            if(stat(file_path,&st)!=0){
                perror("stat");
                continue;
            }
            
            if(!S_ISREG(st.st_mode)){ 
                printf("不是普通文件\n");
                continue;
            }
            char file_name[256];
            memset(file_name,0,sizeof(file_name));
            char *p=strrchr(file_path,'/');
            if(p){
                strcpy(file_name,p+1);
            }else{
                strcpy(file_name,file_path);
            }
            
            FILE* fp = fopen(file_path,"rb");
            if(fp==NULL){
                perror("fopen");
                continue;
            }
            char header[BUF_SIZE];
            int header_len=snprintf(header,sizeof(header),"UPLOAD %s %lld\n",file_name,(long long)st.st_size);
            if(header_len<0||(size_t)header_len>=sizeof(header)){
                printf("上传的命令过长\n");
                fclose(fp);
                continue;
            }
            
            if(send_all(sockfd,header,(size_t)header_len)!=0){
                printf("发送上传命令失败\n");
                fclose(fp);
                break;
            }
            long long remaining=(long long)st.st_size;
            char file_buf[BUF_SIZE];
            while(remaining>0){
                size_t chunk=remaining>BUF_SIZE?BUF_SIZE:(size_t)remaining;
                size_t n=fread(file_buf,1,chunk,fp);
                if(n==0){
                    printf("读取文件失败\n");
                    break;
                }
                if(send_all(sockfd,file_buf,n)!=0){
                    printf("发送文件失败\n");
                    break;
                }
                remaining-=n;
            }
            fclose(fp);            
            if(remaining==0){
                printf("文件上传成功\n");
            }
            continue;
        }
        if (strncmp(write_buf,"/download ",10)==0){
            char file_name[256];
            if(sscanf(write_buf,"/download %255s",file_name)!=1){
                printf("下载命令格式错误\n");
                continue;
            }
            char request[BUF_SIZE];
            int request_len=snprintf(request,sizeof(request),"DOWNLOAD %s\n",file_name);
            if(request_len<0||(size_t)request_len>=sizeof(request)){
                printf("下载请求过长\n");
                continue;
            }
            if(send_all(sockfd,request,(size_t)request_len)!=0){
                printf("发送下载请求失败\n");
                break;
            }
            printf("已发送下载请求: %s\n",file_name);
            continue;
        }

        if(send(sockfd,write_buf,strlen(write_buf),0)<=0){
            perror("send");
            break;
        }
    }
    return NULL;
}

int main(void)
{
    signal(SIGPIPE,SIG_IGN);
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
    fflush(stdout);

    pthread_t pid1,pid2;
    if(pthread_create(&pid1,NULL,read_from_server,(void*)&sockfd)!=0){
        perror("pthread_create");
        close(sockfd);
        return -1;
    }
    if(pthread_create(&pid2,NULL,write_to_server,(void*)&sockfd)!=0){
        perror("pthread_create");
        shutdown(sockfd,SHUT_RDWR);
        pthread_join(pid1,NULL);
        close(sockfd);
        return -1;
    }
    pthread_join(pid1,NULL);
    pthread_join(pid2,NULL);

    printf("释放资源\n");
    close(sockfd);

    return 0;
}
