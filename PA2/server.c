/*
	C socket server, handles multiple clients using threads
*/

#include<stdio.h>
#include<string.h>	//strlen
#include<stdlib.h>	//strlen
#include<sys/socket.h>
#include<arpa/inet.h>	//inet_addr
#include<unistd.h>	//write
#include<pthread.h> //for threading , link with lpthread
#include<errno.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <limits.h>
#include <fcntl.h>
#include <signal.h>

const char *types[9][2] = {
        {".txt", "text/plain"},
        {".html", "text/html"},
        {".htm", "text/html"},
        {".jpg", "image/jpeg"},
        {".png", "image/png"},
        {".ico", "image/vnd.microsoft.icon"},
        {".js", "application/javascript"},
        {".gif", "image/gif"},
        {".css", "text/css"},
};

#define BUFFER_SIZE 8192
#define LISTEN_COUNT 1024

static volatile int keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
}

//the thread function
void *connection_handler(void *);

int main(int argc , char *argv[])
{
	int socket_desc , client_sock , c , *new_sock, options = 1, flags;
	struct sockaddr_in server , client;
    if(argc != 2)
        return 1;
	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		printf("Could not create socket\n");
	}
	printf("Socket created\n");

	// eliminate already in use error
	if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, (const void *) &options, sizeof(int)) < 0)
		return -1;


    //strncmp(filename + strlen(filename) - strlen(types[i][0]), types[i][0], strlen(types[i][0]));
	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( atoi(argv[1]));
	
	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		perror("bind failed. Error");
		return 1;
	}
	printf("bind done\n");

    if ((flags = fcntl(socket_desc, F_GETFL, 0)) < 0)
    {
        return -1;
    }
    if (fcntl(socket_desc, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        return -1;
    }
	
	//Listen
	listen(socket_desc , LISTEN_COUNT);
	
	//Accept and incoming connection
	printf("Waiting for incoming connections...\n");
	c = sizeof(struct sockaddr_in);

	while( keepRunning )
	{
        client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
		new_sock = malloc(sizeof(int));
		*new_sock = client_sock;

		if (*new_sock > 0)
		{
            pthread_t sniffer_thread;
            printf("Connection accepted\n");
            if (pthread_create(&sniffer_thread, NULL, connection_handler, (void *) new_sock) != 0) {
                perror("could not create thread");
                return 1;
            }
        } else {
		    free(new_sock);
		}


		
		//Now join the thread , so that we dont terminate before the thread
		//pthread_join( sniffer_thread , NULL);
	}
	
	if (client_sock < 0)
	{
		perror("accept failed");
		return 1;
	}

	return 0;

}

/*
 * This will handle connection for each client
 * */
void *connection_handler(void *socket_desc)
{
	FILE *file;
	char bufferClient[BUFFER_SIZE];
	char *bufferServer;
	char filename[PATH_MAX];
	bzero(filename, PATH_MAX);
    bzero(bufferClient, BUFFER_SIZE);
	char *data;
	int contentLength;
    char *method, *requestfile, *version, *savepoint;
    int n = 0;
    char contentType[10];
    pthread_detach(pthread_self());

	contentLength = 0;
	int *thread = (int *)socket_desc;
	read(*thread, bufferClient, BUFFER_SIZE);
	printf("Buffer from Client: %s\n", bufferClient);
	if ((strncmp(bufferClient, "GET", 3)) != 0)
	{
		char error[100];
		strcpy(error, "Please enter a valid command such as GET");
		send(*thread, error, 100, 0);
	}
	else
	{
		method = strtok_r(bufferClient, " ", &savepoint); //savepoint is pointer just in case it gets interrupted
		requestfile = strtok_r(NULL, " ", &savepoint);
		version = strtok_r(NULL, "\n", &savepoint);
	    strcpy(filename, ".");
		strcat(filename, requestfile);
		printf("THIS IS THE FILE: %s\n", filename);
        for (int i = 0; i < 9; i++)
        {
            if (strncmp(filename + strlen(filename) - strlen(types[i][0]), types[i][0], strlen(types[i][0])) == 0)
            {
                strcpy(contentType, types[i][1]);
                break;
            }
        }
		file = fopen(filename, "r");
		if (file == NULL)
		{
			bufferServer = (char *)malloc(48);
			strcpy(bufferServer, "HTTP/1.1 500 WEBPAGE NOT FOUND\n\n");
			send(*thread, bufferServer, strlen(bufferServer), 0);
		}
		else
		{
            fseek(file, 0 , SEEK_END);
            contentLength = ftell(file);
            fseek(file, 0, SEEK_SET);
            bufferServer = (char *)malloc(BUFFER_SIZE);
            bzero(bufferServer, BUFFER_SIZE);
            sprintf(bufferServer, "%s 200 Document Follows\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n", version, contentLength, contentType);
            data = (char *)malloc(BUFFER_SIZE);

            send(*thread, bufferServer, strlen(bufferServer), 0);

            do
            {
                bzero(data, BUFFER_SIZE);

                n = fread(data, sizeof(char), BUFFER_SIZE, file);
                n = send(*thread, data, n, 0);
            } while(n == BUFFER_SIZE);
            free(data);
		}
		free(bufferServer);
		fclose(file);
	}
	close(*thread);
	free(thread);
	return NULL;
}
