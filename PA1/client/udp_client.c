/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 * Author: William Anderson
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <ctype.h>

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno, n; //socket, port number, and n-size
    int serverlen; //length of data from server
    struct sockaddr_in serveraddr; //socket struct
    struct hostent *server; //server pointer
    char *hostname; //host name
    char buf[BUFSIZE]; //buffer for use
    char filebuf[BUFSIZE]; //buffer for file

    /* check command line arguments */
    if (argc < 3) {
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

    /* get a message from the user */
    bzero(buf, BUFSIZE);

    //Asking user to enter command for server
    printf("THIS IS A UDP CONNECTION. ENTER COMMAND FOR SERVER: \n");
    printf("get [file-name]\nput [file-name]\ndelete [file-name]\nls\nexit\n");
    printf("-------------------------------------------------------------\n\n");

    //create buffer for the user input, command, and filename
    char input[30], *command, *filename, input2[30];
    fgets(input, 30, stdin);
    strcpy(input2, input);


    /* send the message to the server */
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, input, strlen(input), 0, (const struct sockaddr *)&serveraddr, serverlen);
    if (n < 0) 
      error("ERROR SENDING TO SERVER\n");
   
    //set command and filename from user input
    command = strtok(input, " ");
    filename = strtok(NULL, " ");
    
    //trim filename if it is there (extra chars)
    if(filename != NULL)
    {
        int end = strlen(filename) - 1;
        while(isspace(filename[end])) end --;
        filename[end+1] = '\0';
    }

    //Make sure n is the right size before running cmmd
    n = BUFSIZE;

    //Conditionals for commands from user start here
    if((strcmp(command, "get")) == 0)
    {
	//Zero out buff
	bzero(buf, BUFSIZE);
	
	//open file in write mode
	FILE *file;
	file = fopen(filename, "wb");
	
	//while there is still data to receive	
	while(n == BUFSIZE)
	{
	    //receive buffer for data from server
	    char receive[BUFSIZE];
	    bzero(receive, BUFSIZE);
		
	    /* print the server's reply */
	    n = recvfrom(sockfd, receive, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0)
	        error("ERROR RECEIVING FROM SERVER\n");
	    printf("ECHO FROM SERVER: %s\n", receive);

	    //simply write to buffer and close file
	    if(fwrite(receive, n, 1, file) < 0)
	    {
		printf("ERROR WRITING FILE\n");
		exit(1);
	    }
	}
	
	//remember to close file
	fclose(file);
    }
    else if((strcmp(command, "put")) == 0)
    {
	//Open file and check if its there
	FILE *file;
	file = fopen(filename, "rb");
	if(file == NULL)
	{
	    printf("FILE NOT HERE\n");
	    exit(1);
	}
	
	//while there is data still left to send
	while(n == BUFSIZE)
	{
	    //zero out buf for file
       	    bzero(filebuf, BUFSIZE);

	    //read file into buf and check it
	    if((n = fread(filebuf, 1, BUFSIZE, file)) < 0)
	    {
		printf("UNABLE TO READ FILE INTO BUFFER\n");
		exit(1);
	    }
	 
	    //Send file through the socket
	    n = sendto(sockfd, filebuf, n, 0, (const struct sockaddr *)&serveraddr, serverlen);
            if (n < 0)
                error("ERROR SENDING TO SERVER\n");
	}

	//gotta remember to close the file
	fclose(file);
    }
    else if((strcmp(command, "ls\n")) == 0)
    {
	//receive buffer for data from server
	char receive[BUFSIZE];
        bzero(receive, BUFSIZE);

	/* print the server's reply */
        n = recvfrom(sockfd, receive, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
        if (n < 0)
            error("ERROR RECEIVING FROM SERVER\n");
        printf("ECHO FROM SERVER: %s\n", receive);	
    }
    else if((strcmp(command, "exit\n")) == 0)
    {
	//receive buffer for data from server
	char receive[BUFSIZE];
        bzero(receive, BUFSIZE);

        /* print the server's reply */
        n = recvfrom(sockfd, receive, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
        if (n < 0)
            error("ERROR RECEIVING FROM SERVER\n");
        printf("ECHO FROM SERVER: %s\n", receive);
    }
    else
    {
	//receive buffer for data from server
	char receive[BUFSIZE];
        bzero(receive, BUFSIZE);
        
	/* print the server's reply */
        n = recvfrom(sockfd, receive, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
        if (n < 0)
            error("ERROR RECEIVING FROM SERVER\n");
        printf("ECHO FROM SERVER: %s\n", receive);
    }

    //gotta remember to close socket
    close(sockfd);

    return 0;
}
