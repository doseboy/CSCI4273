/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 * Author: William Anderson
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <ctype.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char filebuf[BUFSIZE]; //file buffer
  char *hostaddrp; /* dotted decimal host addr string */
  char *command; //command from client
  char *filename; //filename from client
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
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {
    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    
    //tell user server is up
    printf("SERVER RUNNING...\n");

    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");
   
    //Get command and filename from client via buffer
    char buf2[BUFSIZE];
    strcpy(buf2, buf);
    command = strtok(buf, " ");
    filename = strtok(NULL, " ");
    
    //If the filename is there trim the chars off the end 
    if(filename != NULL)
    {
        int end = strlen(filename) - 1;
        while(isspace(filename[end])) end --;
        filename[end+1] = '\0';
    }

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
    printf("server received datagram from %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    printf("server received %ld/%d bytes: %s\n", strlen(buf), n, buf);
    
    n = BUFSIZE;
    
    
    //read client's commands
    if((strcmp(command, "get")) == 0)
    {
        //Open file in read mode for buffer
	FILE *file;
        file = fopen(filename, "rb");
	if(file == NULL)
        {
            printf("FILE NOT HERE");
            exit(1);
        }

	//Loop until all the packets are through
	while(n == BUFSIZE)
        {
		//zero out buf for file
		bzero(filebuf, BUFSIZE);

		//read file into buf and check it
		if((n = fread(filebuf, 1, BUFSIZE, file)) < 0)
		{
		    printf("UNABLE TO READ FILE");
		    exit(1);
		}

		//Send file through the socket
		n = sendto(sockfd, filebuf, n, 0, (const struct sockaddr *)&clientaddr, clientlen);
		if (n < 0)
		    error("ERROR SENDING TO FILE");
		printf("MESSAGE SENT TO CLIENT\n");
	}

        //gotta remember to close the file
        fclose(file);
    }
    else if((strcmp(command, "put")) == 0)
    {
	//Zero out buff
        bzero(filebuf, BUFSIZE);

        //open file in write mode
        FILE *file;
        file = fopen(filename, "wb");
        
	//while there is still data to receive
	while(n == BUFSIZE)
        {
		char receive[BUFSIZE];
		bzero(receive, BUFSIZE);
		/* print the server's reply */
		n = recvfrom(sockfd, receive, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
		if (n < 0)
		    error("ERROR RECEIVING FROM CLIENT");
		printf("ECHO FROM CLIENT: %s", receive);


		//simply write to buffer and close file
		if(fwrite(receive, n, 1, file) < 0)
		{
		    printf("ERROR WRITING TO BUFFER");
		    exit(1);
		}
	}

        fclose(file);
	printf("RECEIVED FILE\n");
    }
    else if((strcmp(command, "ls\n")) == 0)
    {
	//String sent back to client
	char str[BUFSIZE];
	bzero(str, BUFSIZE);

	//Directory object
	DIR *d;

	//Directory struct
	struct dirent *dir;

	//open current directory for file
	d = opendir(".");
	if(d)
	{
	    //get directory name from dirent struct 
	    //use readdir for confirming directory is true
	    while((dir = readdir(d)) != NULL)
	    {
		strcat(str, dir->d_name);
		strcat(str, "\n");
	    }
	    
	    //send client message that their command worked or not
	    sendto(sockfd, str, strlen(str), 0, (const struct sockaddr *)&clientaddr, clientlen);
	    closedir(d);
	}
	else
	{
	    printf("UNABLE TO USE COMMAND\n");
	}

    }
    else if((strcmp(command, "delete")) == 0)
    {
	FILE *file;

	//open file in read mode
        file = fopen(filename, "r");
        
	//check that the file is there
	if(file == NULL)
        {
            printf("FILE NOT HERE");
            exit(1);
        }

	//tell client their file was deleted
	char str[BUFSIZE];
	bzero(str, BUFSIZE);
	strcpy(str, filename);
	strcat(str, " WAS DELETED");
	int ret = remove(filename);
	if (ret == 0)
        {
	    printf("DELETED: %s SUCCESSFULL\n", filename);
	    
	    //tell client their file was deleted
            char str[BUFSIZE];
	    bzero(str, BUFSIZE);
            strcpy(str, filename);
            strcat(str, " WAS DELETED\n");
	    sendto(sockfd, str, strlen(str), 0 , (const struct sockaddr *)&clientaddr, clientlen);
	}
	else
	{
	    printf("DELETION FAILED");
	    
	    //tell client their file was deleted
            char str[BUFSIZE];
	    bzero(str, BUFSIZE);
            strcpy(str, filename);
            strcat(str, " WAS NOT DELETED\n");
            sendto(sockfd, str, strlen(str), 0 , (const struct sockaddr *)&clientaddr, clientlen);
	}
	
	//remember to close file
	fclose(file);
    }
    else if((strcmp(command, "exit\n")) == 0)
    {
	//string for client
	char str[] = "EXITED SERVER";
	printf("EXIT STATE REACHED\n");		
	
	//tell client youre exited
	sendto(sockfd, str, strlen(str), 0 , (const struct sockaddr *)&clientaddr, clientlen);
	close(sockfd);
	break;
    }
    else
    {
	//same as above but tell user that their command doesn't exist
	printf("else\n");
	char str[] = "COMMAND NOT FOUND: ";
	char combuff[20];
	strcpy(combuff, command);
	strcat(str, combuff);
	sendto(sockfd, str, strlen(str), 0 , (const struct sockaddr *)&clientaddr, clientlen);
    }
  }

  //close the socket!
  close(sockfd);
  return 0;
}
