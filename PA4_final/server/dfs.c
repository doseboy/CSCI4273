#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <dirent.h>
#include <openssl/md5.h>
#include "../common.h"

#define MAXLINE  4096  /* max text line length */

struct Users
{
    char * username[10];
    char * password[10];
    int user_count;
} users;

struct Dfs_info
{
  char dfs_name[20];
  char port[20];
  int dfs_number;
} dfs_info;

void * run_command(void *arg);
int authenticate(char * filename);
int receivePut(char * filename, int s);
int receiveGet(char * filename, int s);


int main(int argc, char * argv[])
{

    pid_t pid;
    int s, connection, port;
    struct sockaddr_in sin;
    char filename[20] = "dfs.conf";

	if (argc > 3)
	{
		printf("Usage: <dfs_name> <port>\n");
		exit(1);
	}

	strcpy(dfs_info.dfs_name, argv[1]);
    strcpy(dfs_info.port, argv[2]);
	dfs_info.dfs_number = dfs_info.dfs_name[3] - '0';

	if(authenticate(filename) < 0) {
        perror("ERROR READING CONFIG");
        return 1;
    }

	bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(atoi(dfs_info.port));
    sin.sin_addr.s_addr = INADDR_ANY;
    int opt;

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("UNABLE TO CREATE SOCKET\n");
    }

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(int)) < 0) {
        close(s);
        perror("Socket option failed");
    }
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("BIND ERROR\n");
        exit(1);
    }

    if( listen(s, 1) < 0)
    {
        printf("LISTEN ERROR\n");
    }

	while(1)
	{
		if((connection = accept(s, NULL, NULL)) < 0)
		    perror("ERROR ACCEPTING\n");

		pthread_t id;
		pthread_create(&id, NULL, run_command, &connection);

	}

	close(s);
	return 0;
}


void * run_command(void *arg)
{
    char buffer[MAXLINE];
    size_t buffer_len = 0;
    int connection = *(int *)arg;
    int part;
    int n, authed = 0;
    char command[30];
    char filename[30];
    char username[30];
    char password[30];

    bzero(buffer, MAXLINE);
    bzero(command, 30);
    bzero(filename, 30);
    bzero(username, 30);
    bzero(password, 30);
    // read data from client into buffer
    if((n = recv(connection, &buffer_len, sizeof(buffer_len), 0)) < 0)
        perror("ERROR READING\n");
    buffer_len = ntohl(buffer_len);
    printf("Client reply %zu\n", buffer_len);
    if((n = recv(connection, buffer, buffer_len, 0)) < 0)
        perror("ERROR READING\n");
    printf("Client reply %s\n", buffer);

    // Get command, username, and password from client
    char check[4];
    strncpy(check, buffer, 4);
    if(strcmp(check,"list") == 0)
        sscanf(buffer, "%s %s %s", command, username, password);
    else
        sscanf(buffer, "%s %s %s %s",command, filename, username, password);

    // check username and password
    for (int u = 0; u < users.user_count && authed == 0; u++)
        if (strcmp(users.username[u], username) == 0 && strcmp(users.password[u], password) == 0)
            authed = 1;

    if (authed == 0) {
        // couldnt find user
        send(connection, badPassword, strlen(badPassword), 0);
        close(connection);
        return NULL;
    }
    else {
        // authentication worked
        send(connection, successAuth, strlen(successAuth), 0);
    }

    if(strcmp(command, "put") == 0)
    {
        receivePut(filename, connection);
    }
    //LIST
    if(strcmp(command, "list") == 0)
    {
        char *listOfFiles = malloc(8192), *to_client;
        ssize_t clientCapcity = 8192;
        bzero(listOfFiles, clientCapcity);
        DIR * dir;
        struct dirent *dir2;

        char dir_created[PATH_MAX];
        sprintf(dir_created, "./%s/%s", dfs_info.dfs_name, username);
        if((dir = opendir(dir_created)) == NULL)
            perror("ERROR OPENING DIRECTORY\n");
        else
        {
            // loop through files i can find
            while((dir2 = readdir(dir)) != NULL)
            {
                if(strcmp(".", dir2->d_name) == 0 || strcmp("..", dir2->d_name) == 0)
                    continue;

                if (strlen(listOfFiles) + strlen(dir2->d_name) + 1 == clientCapcity){
                    clientCapcity *= 2;
                    listOfFiles = realloc(listOfFiles, clientCapcity);
                }
                strcat(listOfFiles, dir2->d_name);
                strcat(listOfFiles, "\n");
            }
            printf("Directory opened...\n");
            closedir(dir);

            to_client = malloc(strlen(listOfFiles) + 20);
            sprintf(to_client, "%019zu%s", strlen(listOfFiles)-1, listOfFiles);

            if (send(connection, to_client, strlen(to_client), 0) < 0)
            {
                perror("ERROR SENDING LIST TO CLIENT");
                exit(1);
            }
        }
    }

    if(strcmp(command, "get") == 0)
    {
        receiveGet(filename, connection);
    }

    close(connection);
    return NULL;
}


int authenticate(char * filename)
{
    char *buffer, *token, *newline;
    FILE *filepointer;
    if((filepointer = fopen(filename, "r")) == NULL) {
        perror("ERROR OPENING FILE");
        return -1;
    }

    buffer = malloc(8192);
    size_t currentSize = 8192;
    users.user_count = 0;

    while(getline(&buffer, &currentSize, filepointer) != -1 && users.user_count < 10)
    {
        users.username[users.user_count] = calloc(sizeof(buffer), sizeof(char));
        users.password[users.user_count] = calloc(sizeof(buffer), sizeof(char));
        token = strchr(buffer, ':');
        token[0] = '\0';
        strcpy(users.username[users.user_count], buffer);

        newline = strchr(token + 1, '\n');
        newline[0] = '\0';
        strcpy(users.password[users.user_count], token + 1);
        printf("Username: %s, Password: %s\n", users.username[users.user_count], users.password[users.user_count]);

        //Mkdir for user in DFS
        struct stat st = {0};
        char dir_created[30];
        sprintf(dir_created, "./%s/%s", dfs_info.dfs_name, users.username[users.user_count]);
        printf("%s\n", dir_created);
        if (stat(dir_created, &st) == -1){
            mkdir(dir_created, 0700);
        }
        users.user_count++;
    }

    free(buffer);
    fclose(filepointer);
    return users.user_count;
}

int receivePut(char * filename, int s)
{
    char * part, partSizeBuffer[20];
    size_t total_data_len;
    int partNum;
    FILE *filepointer;
    size_t n = 0, nameSize = strlen(dfs_info.dfs_name) + strlen(users.username[0]) + strlen(filename) + 5;
    char filenameWithPart[nameSize];

    char receivedPartNum;
    bzero(partSizeBuffer, 20);

    // for each part
    for(int j = 0; j < 2; j++)
    {
        if(recv(s, &receivedPartNum, sizeof(receivedPartNum), 0) < 0)
            perror("ERROR RECEIVEING part designation");

        partNum = (int)strtol(&receivedPartNum, NULL, 10);
        bzero(partSizeBuffer, 20);
        if(recv(s, partSizeBuffer, 19, 0) < 0)
            perror("ERROR RECEIVEING part size");

        total_data_len = strtol(partSizeBuffer, NULL, 10);

        part = malloc(total_data_len + 1);
        bzero(part, total_data_len + 1);

        int n;
        // receive data for part
        if((n = recv(s, part, total_data_len, 0)) < 0)
            perror("ERROR RECEIVING data");

        // build file name
        bzero(filenameWithPart, nameSize);
        sprintf(filenameWithPart, "%s/%s/%s.%d", dfs_info.dfs_name, users.username[0], filename, partNum);
        printf("Part file name %s\n", filenameWithPart);
        if ((filepointer = fopen(filenameWithPart, "w")) != NULL) {
            // write to part
            fwrite(part, 1, total_data_len, filepointer);
            fclose(filepointer);
        }
        else
            perror("Could not read file");


        free(part);
    }

    return 0;
}

int receiveGet(char * filename, int sock)
{
    long file_len;
    size_t nameSize = strlen(dfs_info.dfs_name) + strlen(users.username[0]) + strlen(filename) + 5;
    char filenameWithPart[nameSize];
    FILE * filepointer;
    char *partPtr, sizeBuff[20], partNumChar[2];

    for(int j = 0; j < 4; j++ )
    {
        bzero(filenameWithPart, nameSize);
        // build file name
        sprintf(filenameWithPart, "%s/%s/%s.%d", dfs_info.dfs_name, users.username[0], filename, j);

        if ((filepointer = fopen(filenameWithPart, "r")) == NULL)
            continue;

        fseek(filepointer, 0, SEEK_END);
        file_len = ftell(filepointer);
        fseek(filepointer, 0, SEEK_SET);

        partPtr = malloc(file_len);
        fread(partPtr, file_len, 1, filepointer);

        bzero(sizeBuff, 20);
        sprintf(sizeBuff, "%019zu", file_len);

        partNumChar[1] = '\0';
        sprintf(partNumChar, "%d", j);

        if(send(sock, partNumChar, 1, 0) < 0)
            perror("ERROR WRITING 1\n");
        else if(send(sock, sizeBuff, 19, 0) < 0)
            perror("ERROR WRITING 2\n");
        else if(send(sock, partPtr, file_len, 0) < 1)
            perror("OR SENDING TOTAL FILE\n");
        fclose(filepointer);
    }

    return 0;
}
