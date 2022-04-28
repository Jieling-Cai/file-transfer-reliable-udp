/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <netdb.h> 
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <assert.h>

#define BUFSIZE 30000
#define h_addr h_addr_list[0] /* for backward compatibility */

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

void construct_Msg(char type, char window_size, char filename[20], char*msg){
      memcpy(msg,&type,1);
      memcpy(msg+1,&window_size,1);
      memcpy(msg+2,filename,20);
}


void print_pkt(char *buf){
    char type;
    char seq_no;
    
    memcpy(&type,buf,1);
    memcpy(&seq_no,buf+1,1);

    printf("\nmsg type: %d\n",type);
    printf("msg sequence no: %d\n",seq_no);
}

int file_size(char* filename)
{
    struct stat statbuf;
    stat(filename,&statbuf);
    int size=statbuf.st_size;
    return size;
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char *buf=(char*)malloc(BUFSIZE);

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* send the message to the server */
    serverlen = sizeof(serveraddr);
    char type = 1;
    char window_size = 9;
    char filename[20] = "message.txt";

    char *msg = (char*)malloc(22);
    construct_Msg(type, window_size, filename, msg);

    n = sendto(sockfd, msg, 22, 0, &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR in sendto");
    
    int n2=0;
    char *pkt = (char*)malloc(514);
    char *ack = (char*)malloc(2);

    char ack_type = 3;

    int n3;
    int n4=0;
    
    char *data = (char*)malloc(BUFSIZE);
    bzero(data,BUFSIZE);

    /* print the server's reply */
    while (1)
    { 
      bzero(buf,BUFSIZE);
      n = recvfrom(sockfd, buf, 514, 0, &serveraddr, &serverlen);
      if(n==1){break;}
      memcpy(data+n4,buf+2,n-2);
      n2+=n;
      n4+=n-2;
      bzero(ack,2);
      memcpy(ack,&ack_type,1);
      memcpy(ack+1,buf+1,1);
      n3 = sendto(sockfd, ack, 2, 0, &serveraddr, serverlen);
      print_pkt(buf);
      printf("cumulative received bytes: %d\n",n2);
      printf("writing %d bytes ACK to server\n",n3);
      if(n<514){break;}
    }

    if(n==1){printf("error type: %d\n",*buf); return 0;}

    FILE *w = fopen("cmp","wb");
    fwrite(data, n4, 1, w);
    // fprintf(w, "%s", data);
    // fputs(data,w);

    fclose(w);

    int size = file_size("cmp");
    printf("received file size: %d\n",size);
}