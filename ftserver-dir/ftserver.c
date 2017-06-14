/*
* server.c -- a stream socket server demo
* Original code from Beej's guide
* and code snippet from Linux Programming by  Michael Kerrisk
*/
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

//#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10     // how many pending connections queue will hold
#define MAXDATASIZE 1024
#define MAX_ARG 2

int recvall(int fd, char* buf, int n);
int sendall(int s, char *buf, int *len);
int send_file(int datafd, char* fn);
int valid_cmd(char* buf, char** params);
int send_dir(int datafd);
int run_server(char* PORT);
void *get_in_addr(struct sockaddr *sa);
void sigchld_handler(int s);
int run_client(struct sockaddr* client_addr, char* DATAPORT);
int start_flow(int new_fd);

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr,"usage: ./ftserver [port]\n");
        exit(1);
    }
    char* port = argv[1];
    run_server(port);
    return 0;
}

int run_server(char* PORT)
{
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        /*
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
            */
        char host[1024];

        getnameinfo( (struct sockaddr *)&their_addr, sin_size,
                host, sizeof host, NULL, 0, NI_NOFQDN );
        printf("server: got connection from %s\n", host);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            start_flow(new_fd);
            close(new_fd);
    printf("\nserver: waiting for connections...\n");
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }
    return 0;
}

int start_flow(int new_fd)
{
    int numbytes;
    int validargs;
    int datafd;
    char buf[MAXDATASIZE];
    char** params = malloc(MAX_ARG*sizeof(char*));
    int i;
    char* DATAPORT;
    char file[MAXDATASIZE];
    struct sockaddr client_addr;
    socklen_t client_addrlen;

    if ( getpeername(new_fd, &client_addr, &client_addrlen) < 0)
        perror("getpeername");

    /* get the command string from client */
    numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0);
    //numbytes = recvall(new_fd, buf,  MAXDATASIZE-1);
    buf[numbytes-1] = '\0';
    fprintf(stdout, "Received: %s\n", buf);

    /* parse cmd string */
    validargs = valid_cmd(buf, params);
    DATAPORT = params[0];
    memset(file,MAXDATASIZE, '\0');
    sprintf(file, "\"%s\"", params[2]);

    /* send data */
    if (params[1][1]=='l' && validargs > 0) {
        /* send a list of directory */
        fprintf(stdout, "Sending directory\n");
        if (send(new_fd, "list data\n", 10, 0) == -1)
            perror("send list response");
        sleep(1);
        datafd = run_client(&client_addr, DATAPORT);
        send_dir(datafd);
        close(datafd);
    } else if (params[1][1]=='g' && validargs > 0) {
        /* send the request file */
        if (send(new_fd, "get data\n", 9, 0) == -1)
                perror("send get response");
        sleep(1);
        datafd = run_client(&client_addr, DATAPORT);
        send_file(datafd, file);
        close(datafd);
    } else if (params[1][1]=='g' && validargs == 0) {
        /* send error back to client */
        if (send(new_fd, "get error\n", 10, 0) == -1)
            perror("send get error response");
        sleep(1);
        if (send(new_fd, "file not found\n", 15, 0) == -1)
            perror("send get error msg");
    }

    /* free memory */
    for ( i = 0; i < validargs; i++){
        free(params[i]);
    }
    free(params);
    return 0;
}

int send_file(int datafd, char* fn){

    char buffer[MAXDATASIZE];
    size_t wbreak = MAXDATASIZE;
    int status;
    int nbuf;
    FILE *fdp; 

    /* open file */
    if ((fdp = fopen("dummy.c", "r")) == NULL) {                                   
        perror("error opening file");                                  
    } 

    /* get signal from client to send */
    memset(buffer, '\0', sizeof(buffer));
    if (!(nbuf = recv(datafd, buffer, MAXDATASIZE, 0)))
        return 0;

    /* read file */
    memset(buffer, '\0', sizeof(buffer));
    nbuf = fread(buffer, sizeof(char), wbreak, fdp);

    /* continue reading tilL EOF */
    while ( nbuf == wbreak ) {

      /* send plaintext */
      sendall(datafd, buffer, &nbuf);
      /* wait for confirmation */
      memset(buffer, '\0', sizeof(buffer));
      nbuf = recv(datafd, buffer, MAXDATASIZE, 0);
      /* clear buffer for next loop */
      memset(buffer, '\0', sizeof(buffer));
      nbuf = fread(buffer,sizeof(char), wbreak, fdp);
    }

    /* send the last bits */
    strcat(buffer, "@@");
    nbuf += 2;
    sendall(datafd, buffer, &nbuf);
}

int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

int recvall(int fd, char* buf, int n)
{
    int numRead;                    /* # of bytes fetched by last read() */
    int totRead;                     /* Total # of bytes read so far */
    numRead = recv(fd, buf, n, 0);
    totRead += numRead;
    buf += numRead;
    while ( (numRead = recv(fd, buf, n, 0)) > 0){
        totRead += numRead;
        buf += numRead;
    }
    return totRead;
}

int send_dir(int datafd){
        DIR           *d;
        struct dirent *dir;
        char fname[MAXDATASIZE];
        d = opendir(".");
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                memset(fname, '\0', MAXDATASIZE);
                sprintf(fname, dir->d_name);
                fname[strlen(fname)] = '\n';
                //printf("%s\n", dir->d_name);
                if (fname[0] != '.'){
                if (send(datafd, fname, strlen(fname), 0) == -1)
                    perror("send list data");
                }
            }
            closedir(d);
        }
        if (send(datafd, "@@\n", 3, 0) == -1)
            perror("send list data");
}

int run_client(struct sockaddr* client_addr, char* DATAPORT)
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    char temp[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    //char s[INET6_ADDRSTRLEN];

   /* tcp connection setup */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_addr = client_addr;
    if ((rv = getaddrinfo(NULL, DATAPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    p = servinfo;
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
        perror("client: socket");
    }
    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        perror("client: connect");
        close(sockfd);
    }
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
    return sockfd;
}

int valid_cmd(char* buf, char** params)
{
    /* extract DATA-PORT, CMD, FILENAME */
    char* temp = strtok(buf, " ,");
    int num = 0;
    FILE *fp;

    while (temp != NULL && num < MAX_ARG) {
        //param = realloc(param, num * sizeof(char*));
        char* buffer = malloc(MAXDATASIZE);
        memset(buffer,'\0', MAXDATASIZE);
        snprintf(buffer, MAXDATASIZE, "%s", temp);
        params[num] = buffer;

        temp = strtok(NULL, " ,");
        num++;

        if (num==2 && params[1][1]=='g'){
            //fprintf(stdout, "%s\n", temp);
            fp = fopen(temp, "r");
            if (fp == NULL){  
                fprintf(stdout,"cannot find file %s\n", temp);
                return 0 ;
            }else {
                fprintf(stdout, "Sending file %s\n", temp);
                fclose(fp);
            }
        }
    }
    return num;
}



void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

