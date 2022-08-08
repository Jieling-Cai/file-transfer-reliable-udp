#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/select.h>
#include <time.h>

#define BUFSIZE 1048576
#define PKTSIZE 512 // only refers to the data part bytes (does not include header bytes)
#define Wait_Time 3

typedef struct __attribute__((__packed__)) rrqMsg{
char type;
char win_size;
char fileName[20];
}rrqMsg;

typedef struct __attribute__((__packed__)) ackMsg{
char type;
char seq_num;
}ackMsg;

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int file_size(char* filename)
{
    struct stat statbuf;
    stat(filename,&statbuf);
    int size=statbuf.st_size;
    return size;
}

int copy_file_to_buf(char filename[20],char* buf){
  FILE *r=fopen(filename,"rb");
  if(r==NULL){return -1;}
  int size = file_size(filename);
  printf("Requested file size: %d\n",size);
  fread(buf,1,size,r);
  fclose(r);
  return size;
}

void construct_pkt(char seq_num, char* buf, char* pkt, int pkt_size){
  char type=2;
  memcpy(pkt,&type,1);
  memcpy(pkt+1,&seq_num,1);
  memcpy(pkt+2,buf+seq_num*PKTSIZE, pkt_size);
}

time_t transmit_current_window(time_t t, char start_seq, int pkt_num, char window_size, char* buf, int sockfd, struct sockaddr_in clientaddr, int clientlen, int fileSize){
    int n;
    time_t timestamp;
    
    if(start_seq>=pkt_num){return t;}
    /* indicate current window is the  final window */
    if(start_seq+window_size>=pkt_num){
              char* pkt = (char*)malloc(PKTSIZE);

              for(char i=start_seq;i<pkt_num;i++){   
                bzero(pkt,PKTSIZE);

                // send previous data packets
                if(i<pkt_num-1){
                  construct_pkt(i, buf, pkt, PKTSIZE);
                  if(i==start_seq){timestamp = time(NULL);}
                  n = sendto(sockfd, pkt, PKTSIZE+2, 0, 
                        (struct sockaddr *) &clientaddr, clientlen);
                  // set timestamp for pkt with current seq_num and add to list
                }
                // send last data packet
                else{
                  // file size is a multiple of each packet size
                  if(fileSize%PKTSIZE==0){
                    printf("file size is a multiple of the fixed packet size\n");
                    construct_pkt(i, buf, pkt, 0); 
                    if(i==start_seq){timestamp = time(NULL);} 
                    n = sendto(sockfd, pkt, 2, 0, 
                    (struct sockaddr *) &clientaddr, clientlen);    
                  }
                  // file size is not a multiple of each packet size
                  else{
                    construct_pkt(i, buf, pkt, fileSize%PKTSIZE);  
                    if(i==start_seq){timestamp = time(NULL);}
                    n = sendto(sockfd, pkt, fileSize%PKTSIZE+2, 0, 
                    (struct sockaddr *) &clientaddr, clientlen);
                  }    
                }
                if (n < 0) 
                  error("ERROR in sendto");
                
              } // end transmit for loop
              free(pkt);
    } // end if
    /* indicate current window is not the final window */
    else{
         char* pkt = (char*)malloc(PKTSIZE);
         for(char i=start_seq;i<start_seq+window_size;i++){            
                bzero(pkt,PKTSIZE);
                construct_pkt(i, buf, pkt, PKTSIZE);
                if(i==start_seq){timestamp = time(NULL);}
                n = sendto(sockfd, pkt, PKTSIZE+2, 0, 
                        (struct sockaddr *) &clientaddr, clientlen);
         }
         free(pkt);
    } // end else
    
    return timestamp;
}


int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char *buf; /* message buf */
  char *buf2;
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram
   */
  clientlen = sizeof(clientaddr);
  buf = (char*)malloc(BUFSIZE);
  buf2 = (char*)malloc(BUFSIZE);

  rrqMsg *RRQ=malloc(sizeof(rrqMsg));
  ackMsg *ACK=malloc(sizeof(ackMsg));

  fd_set master_set;

  /* possibly start loop from here if want */
  while(1){
      /* recvfrom: receive a UDP datagram from a client */
      bzero(buf, BUFSIZE);
      n = recvfrom(sockfd, buf, BUFSIZE, 0,
      (struct sockaddr *) &clientaddr, &clientlen);
      if (n < 0)
        error("ERROR in recvfrom");

      /* 
      * gethostbyaddr: determine who sent the datagram
      */
      hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
          sizeof(clientaddr.sin_addr.s_addr), AF_INET);
      if (hostp == NULL)
        error("ERROR on gethostbyaddr");
      hostaddrp = inet_ntoa(clientaddr.sin_addr);
      if (hostaddrp == NULL)
        error("ERROR on inet_ntoa\n");
      
      printf("\n**********************************************************************\n");
      printf("server received %d bytes read request message from client\n", n);
      
      bzero(RRQ,sizeof(rrqMsg));
      memcpy(RRQ, buf, sizeof(rrqMsg));

      bzero(buf,BUFSIZE);
      int fileSize = copy_file_to_buf(RRQ->fileName,buf);
      if(fileSize==-1){
          char err=4;
          n = sendto(sockfd, &err, 1, 0, 
                      (struct sockaddr *) &clientaddr, clientlen);
      }
      else{   
          /* calculate necessary packet number for transmitting this file */
          int pkt_num = fileSize/PKTSIZE+1;

          /* transmit first window */
          time_t t;
          char start_seq=0;
          t = transmit_current_window(t, start_seq, pkt_num, RRQ->win_size, buf, sockfd, clientaddr, clientlen, fileSize);

          int counter=0;
          char temp_seq;
          
          /* start to check ACKs */
          while(1){
            struct timeval *time_val = malloc(sizeof(struct timeval));
            time_val->tv_sec=Wait_Time;
            time_val->tv_usec=0;       
            
            FD_ZERO(&master_set);
            FD_SET(sockfd, &master_set);
            int receive = select(sockfd+1, &master_set, NULL, NULL, time_val);
            
            if(receive == -1){
              perror("Error occurred in select()");
            }
            else if (receive == 0) // do not receive any ACK after a timeout
            {
              printf("\n**********************************************************************\n");
              // retransmission the current window and reset time
              if(counter==0){temp_seq=start_seq;}
              if(temp_seq==start_seq)
              {
                counter++;
              }
              else{counter=1;temp_seq=start_seq;}
              
              if(counter<5){
                printf("\nserver timeout %d for sequence number %d~%d, start retransmitting the current window\n", counter, start_seq, start_seq+RRQ->win_size-1);
                t = transmit_current_window(t, start_seq, pkt_num, RRQ->win_size, buf, sockfd, clientaddr, clientlen, fileSize);
              }
              else{
                printf("\nAfter 5 consecutive timeouts (for the same sequence number), the server stops the communication\n");
                break;
              }
            }
            else {            
                bzero(buf2, BUFSIZE);
                n = recvfrom(sockfd, buf2, BUFSIZE, 0,
                (struct sockaddr *) &clientaddr, &clientlen);
                if (n < 0)
                  error("ERROR in recvfrom");
                
                bzero(ACK, sizeof(ackMsg));
                memcpy(ACK, buf2, sizeof(ackMsg));
                printf("server received %d bytes ACK %d from client\n", n, (int)ACK->seq_num);

                // move window only when in-order receiving ACK (small to large sequence number, although may lose middle ACK)
                if(ACK->seq_num>=start_seq){
                  for(char i=start_seq;i<=ACK->seq_num;i++){
                  // update start_seq
                  start_seq++;
                  // receive the right most ACK, meaning all pckts received by client, then break the loop and wait for another client request
                  if(start_seq>=pkt_num){break;}
                  // move window and reset time
                  t = transmit_current_window(t, start_seq+RRQ->win_size-1, pkt_num, 1, buf, sockfd, clientaddr, clientlen, fileSize);
                  }
                  if(start_seq>=pkt_num){break;}
                }
                //receive old ACK(useless) within the select timeout, check whether it is really timeout
                else if (time(NULL)-t>=Wait_Time){ 
                    printf("\n**********************************************************************\n");
                    // retransmission the current window and reset time
                    if(counter==0){temp_seq=start_seq;}
                    if(temp_seq==start_seq)
                    {
                      counter++;
                    }
                    else{counter=1;temp_seq=start_seq;}
                    
                    if(counter<5){
                      printf("\nserver timeout %d for sequence number %d~%d, start retransmitting the current window\n", counter, start_seq, start_seq+RRQ->win_size-1);
                      t = transmit_current_window(t, start_seq, pkt_num, RRQ->win_size, buf, sockfd, clientaddr, clientlen, fileSize);
                    }
                    else{
                      printf("\nAfter 5 consecutive timeouts (for the same sequence number), the server stops the communication\n");
                      break;
                    }
                }
            } 
            free(time_val);
          } // end while
      } // end else
      printf("\n\n\nCurrent client's file transfer request ended\n\n\n");
  }
}
