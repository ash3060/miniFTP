#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFSIZE 1024
#define server_port 6666

int getSocket(int* port){
    int ret;
    register int s_socketfd;        // server 描述符
    struct sockaddr_in local;   // 本地地址

    if ((s_socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        fprintf(stderr, "Fail to initial socket!\n");
        exit(1);
    }
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(*port);

    if ((ret = bind(s_socketfd, (struct sockaddr *)&local, sizeof(local))) == -1){
        fprintf(stderr, "Bind unseccessfully!\n");
        exit(2);
    }
    int len = sizeof(local);
    getsockname(s_socketfd,(struct sockaddr*)&local, (socklen_t *)&len);
    *port = ntohs(local.sin_port);
    return s_socketfd;
}

void work(int c_socketfd, int number){
    char buf[BUFSIZE];
    int dataSockfd;
    char file[BUFSIZE];
    int rs,src,target,k;

    char root[BUFSIZE];
    FILE* fp=popen("pwd","r");
    fgets(root,BUFSIZE,fp);
    pclose(fp);

    while(1){
        if(read(c_socketfd,buf,BUFSIZE)>0){
            printf("Command from client %d : %s\n", number, buf);
        }
        //quit
        if(buf[0] == 'Q' && buf[1] == 'U' && buf[2] == 'I' && buf[3] == 'T'){
            printf("Client %d quits", number);
            return;
        }
        //pwd
        else if (buf[0] == 'P' && buf[1] == 'W' && buf[2] == 'D'){
            FILE* fp=popen("pwd","r");
            if((fgets(file,BUFSIZE,fp)) != NULL){
                printf("Current directory is : %s", file);
                write(c_socketfd, (char *)(file), BUFSIZE);
            }
            pclose(fp);
        }
        //dir
        else if (buf[0] == 'D' && buf[1] == 'I' && buf[2] == 'R'){
            FILE * pf=popen("ls","r");
            while(fgets(file,BUFSIZE,pf) != NULL){
                write(c_socketfd,file,BUFSIZE);
            }
            pclose(pf);
            write(c_socketfd,"\n",BUFSIZE);
        }
        //cdup
        else if (buf[0] == 'C' && buf[1] == 'D' && buf[2] == 'U' && buf[3] == 'P'){
            FILE* fp=popen("pwd","r");
            if((fgets(file,BUFSIZE,fp)) != NULL){
                if (strcmp(file, root) == 0){
                    printf("Already root dir!\n");
                    write(c_socketfd,"\n",BUFSIZE);
                    continue;
                }
                int i;
                for (i = 0; file[i] != '\0'; i++){}
                for (; file[i] != '/'; i--){}
                file[i] = '\0';
            }
            pclose(fp);
            rs = chdir(file);
            if(rs == 0){
                FILE* fp=popen("pwd","r");
                if((fgets(file,BUFSIZE,fp)) != NULL){
                    printf("Current directory is : %s", file);
                    write(c_socketfd,(char *)(file),BUFSIZE);
                }
                pclose(fp);
            }
            else{
                write(c_socketfd,"\n",BUFSIZE);
            }
        }
        //cd
        else if (buf[0] == 'C' && buf[1] == 'D'){
            rs = chdir((char*)(buf+3));
            if(rs== 0){
                FILE* fp=popen("pwd","r");
                if((fgets(file,BUFSIZE,fp)) != NULL){
                    printf("Current directory is : %s", file);
                    write(c_socketfd,(char *)(file),BUFSIZE);
                }
                pclose(fp);
            }
            else{
                write(c_socketfd,"\n",BUFSIZE);
            }
        }
        //get
        else if(buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T'){
            strcpy(file, (char *)(buf+4));
            if((src = open(file,O_RDONLY))<0){
                printf("file %s is not exist!\n", file);
                write(c_socketfd,"ERROR",BUFSIZE);
            }
            else{
                int port = 0;
                dataSockfd = getSocket(&port);
                listen(dataSockfd, 20);
                sprintf(buf, "%d", port);
                write(c_socketfd, buf, BUFSIZE);    // send "<dataSocketPort>"
                read(c_socketfd, buf, BUFSIZE); // receive "READY" or "ERROR"
                //printf("here %s\n",buf);
                if (buf[0] == 'E'){
                    close(src);
                    close(dataSockfd);
                    continue;
                }
                struct sockaddr_in client_in;       // client address
                // accept
                int len = sizeof(struct sockaddr_in);
                int csk;
                if ((csk = accept(dataSockfd,
                    (struct sockaddr *)&client_in, (socklen_t*)&len)) == -1){
                    printf("accept error\n");
                }
                lseek(src, 0, SEEK_SET);
                memset(buf, 0x0 ,BUFSIZE);
                while((k = read(src,buf,BUFSIZE)) > 0){
                    write(csk, buf, k);
                }
                write(csk, 0, 0);
            }
            close(src);
            close(dataSockfd);
            printf("Loading finishes\n");
        }
        //put
        else if(buf[0] == 'P' && buf[1] == 'U'&& buf[2] == 'T')
        {
            strcpy(file, (char *)(buf+4));
            FILE * res=popen("ls","r");
            int isExist = 0;
            while(fgets(buf,BUFSIZE,res) != NULL){
                int i;
                for (i = 0; buf[i] != '\n'; i++){}
                buf[i] = '\0';
                if (strcmp(buf, file) == 0){
                    printf("File %s has already existed!\n", file);
                    write(c_socketfd, "ERROR", BUFSIZE);    // send "ERROR"
                    isExist = 1;
                    break;
                }
            }
            if (isExist == 1)
                continue;
            pclose(res);

            if((target = open(file, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0){
                printf("error to write to file %s!\n", file);
                write(c_socketfd, "ERROR", BUFSIZE);    // send "ERROR"
                continue;
            }
            else{
                int port = 0;
                dataSockfd = getSocket(&port);
                listen(dataSockfd, 20);
                sprintf(buf, "%d", port);
                write(c_socketfd, buf, BUFSIZE);    // send "<dataSocketPort>"

                struct sockaddr_in client_in;       // client address
                // accept
                int len = sizeof(struct sockaddr_in);
                int csk;
                if ((csk = accept(dataSockfd, (struct sockaddr *)&client_in, (socklen_t*)&len)) == -1){
                    printf("accept error\n");
                }
                while ((k = read(csk, buf, BUFSIZE)) > 0){
                    write(target, buf, k);
                    if (k != BUFSIZE)
                        break;
                }
            }
            close(target);
            close(dataSockfd);
            printf("Puting has finished\n");
        }


    }

}

int main(int argc, char *argv[]){
    char command[BUFSIZE];      // 接收的命令

    int clientNum = 0;      // client数
    int ret;
    int s_socketfd;         // server socket描述符
    int c_socketfd;     // client socket描述符
    char buf[BUFSIZE];                  //
    int port = server_port;
    s_socketfd = getSocket(&port);

    // listen
    listen(s_socketfd, 20);
    while (1){
        struct sockaddr_in client_in;       // client address
        int len = sizeof(struct sockaddr_in);
        if ((c_socketfd = accept(s_socketfd, (struct sockaddr *)&client_in, (socklen_t*)&len)) == -1){
            printf("accept error\n");
        }

        clientNum++;
        sprintf(buf, "%d", clientNum);
        printf("Client %s, address: %s\n", buf, inet_ntoa(client_in.sin_addr));

        int pid;                   //多线程
        switch (pid = fork()){
        case 0:
            close(s_socketfd);
            write(c_socketfd, buf, BUFSIZE);  //tell the client which number it is
            work(c_socketfd, clientNum);
            close(c_socketfd);
            exit(1);
        case -1:
            printf("fork error\n");
            break;
        default:
            close(c_socketfd);
            break;
        }
    }
    return 0;
}
