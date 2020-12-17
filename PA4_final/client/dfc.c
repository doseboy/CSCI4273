#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/md5.h>
#include <limits.h>
#include "../common.h"

#include <ctype.h>

#define MAXLINE 4096

struct dfs_info
{
    char * name[4];
    char * ip [4];
    char * port[4];
    char * username;
    char * password;
    char * files[10];
} dfs_info;

int authenticate(char * filename);
int get_command(char * filename, int sock[], const int success[]);
int put_command(char * filename, int sock[], const int success[]);
int list_command(int sock[], const int success[]);

int main(int argc, char *argv[])
{
    char config[100];
    int n;
    int s[4], success[4];
    struct sockaddr_in dfs[4];

    for(int i = 0; i < 4; i++)
        success[i] = 1;

    if(argc != 2)
    {
        printf("USAGE: <config>\n");
        exit(1);
    }
    else
    {
        strcpy(config, argv[1]);
    }

    if(authenticate(config) < 0)
        printf("AUTHENTICATION ERROR\n");

    for(int i = 0; i < 4; i++)
    {
        bzero(&dfs[i], sizeof(dfs[i]));
        dfs[i].sin_family = AF_INET;
        dfs[i].sin_port = htons(atoi(dfs_info.port[i]));
        dfs[i].sin_addr.s_addr = inet_addr(dfs_info.ip[i]);
        socklen_t size;

        if((s[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            printf("SOCKET CREATION ERROR\n");

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        if(setsockopt(s[i], SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
            printf("SETSOCKOPT ERROR\n");
        if(setsockopt(s[i], SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
            printf("SETSOCKOPT ERROR\n");

        if(connect(s[i], (struct sockaddr *)&dfs[i], sizeof(dfs[i])) < 0)
        {
            perror("PROBLEM CONNECTING...");
            success[i] = 0;
        }
    }

    char input[100], command[10], filename[20], introduction[100];
    bzero(input, 100);
    bzero(command, 10);
    bzero(filename, 20);
    bzero(introduction, 100);
    fgets(input, 100, stdin);
    sscanf(input, "%s %s", command, filename);
    strtok(input, "\n");
    sprintf(introduction, "%s %s %s", input, dfs_info.username, dfs_info.password);
    printf("%s\n", introduction);
    size_t introduction_length = htonl(strlen(introduction));

    for(int i = 0; i < 4; i++)
    {
        if (success[i] == 0)
            continue;

        if(send(s[i], &introduction_length, sizeof(introduction_length), 0) < 0) //send number of bytes
        {
            perror("ERROR WRITING TO SOCKET");
            success[i] = 0;
        }
        else if(send(s[i], introduction, strlen(introduction), 0) < 0)
        {
            perror("ERROR WRITING TO SOCKET");
            success[i] = 0;
        }
        else {
            // get server authentication response
            bzero(input, 100);
            recv(s[i], input, strlen(badPassword), 0);

            if (strcmp(badPassword, input) == 0)
                success[i] = 0;
            else
                success[i] = 1;
        }
    }

    if((strcmp(command, "get")) == 0)
    {
        get_command(filename, s, success);
    }
    else if((strcmp(command, "put")) == 0)
    {
        put_command(filename, s, success);
    }
    else if((strcmp(command, "list")) == 0)
    {
        list_command(s, success);
    }

    return 0;
}

int authenticate(char * filename)
{
    char buffer[200];
    FILE * filepointer;
    char username[30];
    char password[30];
    char ip_port[30];

    if((filepointer = fopen(filename, "r")) == NULL)
        perror("FILE OPENING ERROR");

    int index = 0;
    while(!feof(filepointer))
    {
        fgets(buffer, 200, filepointer);
        if(strstr(buffer, "Server"))
        {
            dfs_info.ip[index] = malloc(INET_ADDRSTRLEN);
            dfs_info.name[index] = calloc(sizeof(buffer), sizeof(char));
            dfs_info.port[index] = calloc(sizeof(buffer), sizeof(char));
            sscanf(buffer, "%s %s %s", username, password, ip_port);
            char * token = strtok(ip_port, ":");
            strcpy(dfs_info.ip[index], token);
            token = strtok(NULL, ":");
            strcpy(dfs_info.port[index], token);
            printf(" %s %s\n", dfs_info.ip[index], dfs_info.port[index]);
            index++;
        }
        else if(strstr(buffer, "Username"))
        {
            dfs_info.username = calloc(sizeof(buffer), sizeof(char));
            sscanf(buffer, "%s", username);
            char * token = strtok(username, ":");
            token = strtok(NULL, ":");
            strcpy(dfs_info.username, token);
            printf("%s\n", dfs_info.username);
        }
        else if(strstr(buffer, "Password"))
        {
            dfs_info.password = calloc(sizeof(buffer), sizeof(char));
            sscanf(buffer, "%s", password);
            char * token = strtok(password, ":");
            token = strtok(NULL, ":");
            strcpy(dfs_info.password, token);
            printf("%s\n", dfs_info.password);
        }
        else
        {
            printf("ERROR READING CONFIG\n");
            return -1;
        }
    }
    fclose(filepointer);
    return 0;
}

int get_command(char * filename, int s[], const int success[])
{
    char total_data[MAXLINE];
    char * part[4], partSizeBuffer[20];
    size_t total_data_len[4];
    int partNum;
    char partNumChar[2];

    bzero(partSizeBuffer, 20);
    // init part array to NULL
    for (int i = 0; i < 4; i++)
        part[i] = NULL;

    // for each DFS
    for(int i = 0; i < 4; i++)
    {
        if (success[i] == 0)
            continue;

        // for each part
        for(int j = 0; j < 2; j++)
        {
            bzero(partSizeBuffer, 20);
            partNumChar[1] = '\0';
            if(recv(s[i], &partNumChar, 1, 0) < 0)
                printf("ERROR RECEIVEING FILE\n");
            partNum = (int)strtol(partNumChar, NULL, 10);

            if(recv(s[i], partSizeBuffer, 19, 0) < 0)
                printf("ERROR RECEIVEING FILE\n");

            total_data_len[partNum] = strtol(partSizeBuffer, NULL, 10);

            //Allocate memory for new part
            if (part[partNum] == NULL)
                part[partNum] = malloc(total_data_len[partNum] + 1);

            int n;
            bzero(part[partNum], total_data_len[partNum] + 1);
            if((n = recv(s[i], part[partNum], total_data_len[partNum], 0)) <= 0)
                printf("ERROR RECEIVING FILE\n");

        }
    }


    for(int i = 0; i < 4; i++)
    {
        printf("Parts: %s\n", part[i]);
    }
    printf("\n");

    int incomplete = 0;
    for(int i = 0; i < 4; i++)
        if (part[i] == NULL)
            incomplete = 1;

    if (incomplete == 0) {
        FILE *fp = fopen(filename, "w");
        if(fp == NULL)
        {
            printf("FILE DOESN'T EXIST\n");
        }

        for(int i = 0; i < 4; i++)
            fwrite(part[i], total_data_len[i], 1, fp);
        fclose(fp);
    }
    else
        printf("INCOMPLETE FILE\n");

    for (int i = 0; i < 4; i++)
        if(part[i] != NULL)
            free(part[i]);

    return 0;
}

int put_command(char * filename, int sock[], const int success[])
{
    int block;
    long file_len;
    char * part[4], longSize[20];
    unsigned char out[MD5_DIGEST_LENGTH];
    MD5_CTX mdContext;
    unsigned char * hash;
    unsigned long hash_r;
    FILE * filepointer;

    int md5_table[4][4][2] =
    {
            {
                    {0,1},{1,2},{2,3},{3,0}
            },
            {
                    {3,0},{0,1},{1,2},{2,3}
            },
            {
                    {2,3},{3,0},{0,1},{1,2}
            },
            {
                    {1,2},{2,3},{3,0},{0,1}
            }
    };

    filepointer = fopen(filename, "r");
    if(filepointer == NULL)
    {
        printf("FILE DOES NOT EXIST\n");
        exit(1);
    }

    //Get file size and block size
    fseek(filepointer, 0, SEEK_END);
    file_len = ftell(filepointer);
    fseek(filepointer, 0, SEEK_SET);
    block = file_len / 4;

    //Divide parts based on odd or even file length
    for(int i = 0; i < 4; i++)
    {
        if(i == 3)
            part[i] = malloc(file_len - (3 * block) + 1);
        else
            part[i] = malloc(block + 1);
    }

    //
    int j = 0, numRead;
    MD5_Init (&mdContext);
    for(j = 0; j < 4; j++) {
        size_t seekPosition, partSize;
        if (j == 3) {
            partSize = file_len - (3 * block);
        }
        else
            partSize = block;
        //Seekposition is used to move to the current part within.
        seekPosition = (j * block);
        bzero(part[j], partSize + 1);
        fseek(filepointer, seekPosition, SEEK_SET);
        printf("part %d is size %lu\n", j, partSize);

        printf("Read %ld bytes for part #%d\n", fread(part[j], sizeof(char), partSize, filepointer), j);
        MD5_Update(&mdContext, part[j], partSize);
    }
    MD5_Final(out, &mdContext);

    //Get hash from table
    char temp[4];
    hash = malloc(MD5_DIGEST_LENGTH);
    for (int i=0; i < MD5_DIGEST_LENGTH; i++){
        sprintf(temp, "%02x",(unsigned int)out[i]);
        strcat(hash, temp);
    }

    char first_five[5];
    strncpy(first_five, hash, 5);
    hash_r = (long)strtol(first_five, NULL, 16)%4;

    size_t data_len;
    char transmitPartNum[2];
    transmitPartNum[1] = '\0';
    int part_num;

    //server
    for(int j = 0; j < 4; j++ )
    {
        if (success[j] == 0)
            continue;
        //parts
        for(int t = 0; t < 2; t++)
        {

            part_num = md5_table[hash_r][j][t];
            data_len = (part_num == 3) ? file_len - (3 * block) : block;

            printf("%d %zu \"%s\"\n", part_num, data_len, part[part_num]);

            bzero(longSize, 20);

            sprintf(longSize, "%019zu", data_len);

            sprintf(transmitPartNum, "%d", part_num);
            if(send(sock[j], transmitPartNum, sizeof(char), 0) < 0)
                perror("ERROR WRITING 1\n");
            else if(send(sock[j], longSize, 19, 0) < 0)
                perror("ERROR WRITING 2\n");
            else if(send(sock[j], part[part_num], data_len, 0) < 1)
                perror("ERROR SENDING TOTAL FILE\n");
        }
    }

    fclose(filepointer);
    return 0;
}

int list_command(int s[], const int success[])
{
    char *filename[8192];
    int partExistence[8192][4];
    int n, fileIndex;
    char numberBuffer[20], *savePoint, *receiveBuffer;
    int buffer_len;
    size_t numFiles = 0, receiveBufferSize;

    for(int i = 0; i < 4; i++)
    {
        if (success[i] == 0)
            continue;

        bzero(numberBuffer, 20);

        // number of bytes the server is going to send us
        recv(s[i], numberBuffer, 19, 0);
        receiveBufferSize = strtol(numberBuffer, NULL, 10);
        receiveBuffer = malloc(receiveBufferSize + 1);
        bzero(receiveBuffer, receiveBufferSize + 1);

        // receive file list from server
        recv(s[i], receiveBuffer, receiveBufferSize, 0);

        for (char * token = strtok_r(receiveBuffer, "\n", &savePoint); token != NULL; token = strtok_r(NULL, "\n", &savePoint)) {
            char *lastDot = token + (strlen(token) - 2), currentName;
            int partNumber = (int)strtol(lastDot + 1, NULL, 10);

            //Effectively deletes the dot and the num that follows
            lastDot[0] = '\0';

            // file not there if -1
            fileIndex = -1;
            for (int j = 0; j < numFiles && fileIndex == -1; j++)
                if (strcmp(filename[j], token) == 0)
                    fileIndex = j;

            if (fileIndex == -1 && numFiles == 8192) {
                printf("Too many files!\n");
            }

            //Adding new file to array
            else if (fileIndex == -1) {
                filename[numFiles] = strdup(token);
                for (int j = 0; j < 4; j++)
                    partExistence[numFiles][j] = 0;
                partExistence[numFiles][partNumber] = 1;
                numFiles++;
            }
            else {
                // we found the file, it exists
                partExistence[fileIndex][partNumber] = 1;
            }
        }

        free(receiveBuffer);
    }

    printf("Files:\n");
    for(int i = 0; i < numFiles; i++)
    {
        printf("%s", filename[i]);
        for (int j = 0; j < 4; j++)
            if (partExistence[i][j] == 0) {
                printf(" [INCOMPLETE]");
                j = 4;  // break the loop
            }
        printf("\n");
        free(filename[i]);
    }
    return 0;
}
