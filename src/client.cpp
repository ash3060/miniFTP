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
#include <iostream>

using namespace std;

#define BUFSIZE 1024

void dispatch(char *cmd, int sockfd);

int getSocket(char *address, int port){
    int ret;
    register int socketfd;
    struct sockaddr_in sin;

    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        fprintf(stderr, "fail to initial socket!\n");
        exit(1);
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    inet_pton(AF_INET, address, &sin.sin_addr);
    printf("Server address: %s\n",inet_ntoa(sin.sin_addr));
    printf("Server port: %d\n",ntohs(sin.sin_port));

    int len = sizeof(sin);
    if ((ret = connect(socketfd, (struct sockaddr *)&sin, len)) == -1){
        fprintf(stderr, "can't connect server!\n");
        exit(2);
    }

    return socketfd;
}

void contact(int sockfd){
    char cmd[BUFSIZE];
    memset(cmd, 0, BUFSIZE);
    while (1){
        printf("<ftp Client>: ");
        fgets(cmd,BUFSIZE,stdin);
        dispatch(cmd, sockfd);
    }
}

void quitCmd(int sockfd){
    write(sockfd, "QUIT", BUFSIZE);
    close(sockfd);
    printf("quit!\n");
    exit(0);
}

void pwdCmd(int sockfd){
    write(sockfd, "PWD", BUFSIZE);
    char buf[BUFSIZE] = {0};
    if (!(read(sockfd, buf, BUFSIZE)>0)) {
        printf("Lost connection with server!\n");
        exit(0);
    }
    if (*buf == '\0')
        strcpy(buf, "\n");
    printf("Current server directory is %s", buf);
}

void dirCmd(int sockfd){
    write(sockfd, "DIR", BUFSIZE);
    char buf[BUFSIZE] = {0};
    if (!(read(sockfd, buf, BUFSIZE)>0)) {
        printf("Lost connection with server!\n");
        exit(0);
    }
    printf("\t%s", buf);
    while (read(sockfd, buf, BUFSIZE) > 0){
        if (buf[0] == '\n')
            break;
        printf("\t%s", buf);
    }
}

void cdCmd(char *file, int sockfd){
    char buf[BUFSIZE] = "CD ";
    int i;
    for (i = 0; file[i] != '\0'; i++)
    {
        buf[i+3] = file[i];
    }
    buf[i+3] = '\0';

    write(sockfd, buf, BUFSIZE);   // send "CD <dir>"
    //read(sockfd, buf, BUFSIZE);    // receive "<dir>"
    if (!(read(sockfd, buf, BUFSIZE)>0)) {
        printf("Lost connection with server!\n");
        exit(0);
    }

    if (*buf == '\n')
    {
        printf("fail to change directory\n");
    }
    else
    {
        if (*buf == '\0')
            strcpy(buf, "\n");
        printf("Current server directory is %s", buf);
    }
}

void cdupCmd(int sockfd){
    char buf[BUFSIZE] = {0};
    write(sockfd, "CDUP", BUFSIZE);    // send "CDUP"
    //read(sockfd, buf, BUFSIZE);    // receive "<dir>"
    if (!(read(sockfd, buf, BUFSIZE)>0)) {
        printf("Lost connection with server!\n");
        exit(0);
    }

    if (*buf == '\n'){
        printf("fail to change directory\n");
    }
    else{
        if (*buf == '\0')
            strcpy(buf, "\n");
        printf("Current server directory is %s", buf);
    }
}

void getCmd(char *file, int sockfd)  {
    char buf[BUFSIZE] = "GET ";
    int i;
    for (i = 0; file[i] != '\0'; i++){
        buf[i+4] = file[i];
    }
    buf[i+4] = '\0';

    write(sockfd, buf, BUFSIZE);   // send "GET <file>"
    read(sockfd, buf, BUFSIZE);    // receive "<dataSocketPort>" or "ERROR"
    if (*buf == 'E'){
        printf("file %s is not exist!\n", file);
        return;
    }

    int target;
    if((target = open(file, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0){
        printf("error to write to file %s!\n", file);
        write(sockfd, "ERROR", BUFSIZE);   // send "ERROR"
        return;
    }
    else
        write(sockfd, "READY", BUFSIZE);   // send "READY"

    struct sockaddr_in server;
    int len = sizeof(server);
    getsockname(sockfd,(struct sockaddr*)&server, (socklen_t*)&len);
    register int dataSk = getSocket(inet_ntoa(server.sin_addr), atoi(buf));

    int k;
    printf("downloading...\n");
    while ((k = read(dataSk, buf, BUFSIZE)) > 0){
        write(target, buf, k);
        if (k != BUFSIZE)
            break;
    }
    close(dataSk);
    close(target);
    printf("finished!\n");
}

void putCmd(char *file, int sockfd)  {
   int source;
    if((source = open(file,O_RDONLY)) < 0)
    {
        printf("fail to open file %s!\n", file);
        return;
    }

    char buf[BUFSIZE] = "PUT ";
    int i;
    for (i = 0; file[i] != '\0'; i++)
    {
        buf[i+4] = file[i];
    }
    buf[i+4] = '\0';

    write(sockfd, buf, BUFSIZE);
    read(sockfd, buf, BUFSIZE);    // receive "<dataSocketPort>" or "ERROR"
    if (*buf == 'E')
    {
        printf("file %s is already exist in server!\n", file);
        close(source);
        return;
    }

    struct sockaddr_in server;
    int len = sizeof(server);
    getsockname(sockfd,(struct sockaddr*)&server,(socklen_t*)&len);
    register int dataSk = getSocket(inet_ntoa(server.sin_addr), atoi(buf));

    int k;
    printf("uploading...\n");
    lseek(source, 0, SEEK_SET);
    memset(buf, 0x0 ,BUFSIZE);
    while((k = read(source,buf,BUFSIZE)) > 0)
    {
        write(dataSk, buf, k);
    }
    write(dataSk, 0, 0);

    close(dataSk);
    close(source);
    printf("finished!\n");
}

void helpCmd(){
    printf("?\thelp\n");
    printf("ldir\tlist files in local directory of client\n");
    printf("pwd\tshow current directory of server\n");
    printf("dir\tlist files in current directory of server\n");
    printf("get\tdownload a file from server\n\tget <filename>\n");
    printf("put\tupload a file to server\n\tput <filename>\n");
    printf("cd\tchange current directory of server\n\tcd <subdirectory>\n");
    printf("quit\tquit ftp client\n");
}

void lower(char *cmd){
    int i;
    while (*cmd == '\t' || *cmd == ' ')
        cmd++;

    for (i = 0; cmd[i] != ' '; i++){
        if (cmd[i] >= 'A' && cmd[i] <= 'Z')
            cmd[i] += ('a'-'A');
    }
}

void dispatch(char *cmd, int sockfd)  {
    char *file;
    int r;
    lower(cmd);
    while (*cmd == '\t' || *cmd == ' ')
        cmd++;

    if (*cmd == '?')
    {
        helpCmd();
        return;
    }

    switch(*cmd)
    {
    case 'l': //ldir
        {
            if(*(cmd+1) == 'd' && *(cmd+2) == 'i' &&  *(cmd+3) == 'r')
            {
                char buf[BUFSIZE];

                FILE* pf=popen("pwd","r");
                if((fgets(buf,BUFSIZE,pf)) != NULL){
                    printf("local directory is : %s", buf);
                }

                pf=popen("ls","r");
                while(fgets(buf,BUFSIZE,pf) != NULL)
                {
                    printf("\t%s",buf);
                }
                pclose(pf);
                return;
            }
            else
            {
                printf("Invalid command!\n");
                return;
            }
        }
    case 'q':  //quit
        {
            char * temp = cmd;
            if (temp[0] == 'q' && temp[1] == 'u' && temp[2] == 'i' && *(temp+3) == 't')
            {
                quitCmd(sockfd);
                return;
            }
            else
            {
                printf("Invalid command!\n");
                return;
            }
        }
    case 'p':  //pwd
        {
            char *temp = cmd;
        if (temp[0] == 'p' && temp[1] == 'w' && temp[2] == 'd')
        {
            pwdCmd(sockfd);
            return;
        }
        else if(temp[0] == 'p' && temp[1] == 'u' && temp[2] == 't')
        {
            temp += 3;
            while (temp[0] == '\t' || temp[0] == ' ') {
                temp++;
            }
            if (temp[0] == '\n') {
                printf("put <filename>\n");
                return;
            }
            for (int i = 0; temp[i] != '\0'; i++)
            {
                if (temp[i] == ' ' || temp[i] == '\t')
                {
                    printf("put <fileName>\n");
                    return ;
                }
                if (temp[i] == '\n')
                    temp[i] = '\0';
            }
            putCmd(temp, sockfd);
            return;
        }
        else
            {
                printf("Invalid command!\n");
                return;
            }

    }
    case 'd':  //dir
        {
            char *temp = cmd;
            if (temp[0] == 'd' && temp[1] == 'i' && temp[2] == 'r')
            {
                dirCmd(sockfd);
                return;
            }
            else
            {
                printf("Invalid command!\n");
                return;
            }

        }
        return;
    case 'c':
        {
                if (cmd[1] == 'd') {
                char *tmp = cmd;
                tmp += 2;
                while (tmp[0] == ' ' || tmp[0] == '\t') {
                    tmp++;
                }
                if (tmp[0] == '\n') {
                    printf("cd <subdirectory>\n");
                    return;
                }
                if (tmp[0] == '.' && tmp[1] == '.') {
                    cdupCmd(sockfd);
                    return;
                }
                for (int i = 0; tmp[i] != '\0'; i++){
                    if (tmp[i] == ' ' || tmp[i] == '\t'){
                        printf("cd <subdirectory>\n");
                        return;
                    }
                    if (tmp[i] == '\n')
                        tmp[i] = '\0';
                }
                cdCmd(tmp, sockfd);
                return;

            }
            else
            {
                printf("Invalid command!\n");
                return;
            }

        }
    case 'g':  //get
        {
            char *temp = cmd;
        if(temp[0] == 'g' && temp[1] == 'e' && temp[2] == 't')
        {
            temp += 3;
            while (temp[0] == '\t' || temp[0] == ' ') {
                temp++;
            }
            if (temp[0] == '\n') {
                printf("get <filename>\n");
                return;
            }
            for (int i = 0; temp[i] != '\0'; i++)
            {
                if (temp[i] == ' ' || temp[i] == '\t')
                {
                    printf("get <fileName>\n");
                    return ;
                }
                if (temp[i] == '\n')
                    temp[i] = '\0';
            }
            getCmd(temp, sockfd);
            return;
        }
        else
        {
            printf("Invalid command!\n");
            return;
        }
        }
    default:
        {
            printf("Invalid command!\n");
            return;
        }
    }
}


int main(int argc, char *argv[]){
    if (argc != 3){
        printf("Usage: client <address> <port>\n");
        return 0;
    }

    int target,ret;
    char buf[BUFSIZE];
    register int socketfd = getSocket(argv[1], atoi(argv[2]));

    if(read(socketfd,buf,BUFSIZE) > 0){
        printf("You are Client %s\n",buf);
    }

    contact(socketfd);
    return 0;
}
