/*
    HTTP Web Proxy - a simple web proxy client_addr
    that is capable of relaying HTTP requests 
    from clients to the HTTP client_addrs.
	
    Author: William Anderson
	Class: CSCI 4273
    
    USAGE: webproxy <portno>&
*/

#include<string.h>  //strlen
#include<stdlib.h>  //strlen
#include<sys/socket.h>
#include<arpa/inet.h>   //inet_addr
#include<unistd.h>  //write
#include<pthread.h> //for threading , link with lpthread
#include<errno.h>
#include<sys/types.h>
#include<netinet/in.h>
#include <limits.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <netdb.h>
#include <poll.h>
#include <regex.h>
#include <ctype.h>


#define BUFFER_SIZE 8192
#define LISTEN_COUNT 1024

//Flag: if we receive sigint we stop serving
static volatile int keep_running = 1;

//Cache struct BST
struct cache
{
    char * url;
    void * data;
    size_t length;
    struct cache * left;
    struct cache * right;
};

//Important info for the thread handler!
struct thread_args
{
    int socket_desc;
    struct cache *root;
    pthread_mutex_t *mutex_cache;
};

//For sig int
void intHandler(int dummy) {
    keep_running = 0;
}

//AKA the thread handler
void *connection_handler(void *);
//void trimSpace(char*);
//Initialize cache
struct cache *cinit(char *name, void *data, size_t len);

//Append to cache
void append_cache(struct cache *tree, struct cache *entry);

char *get_hostname(const char *);
int regcomp_d(regex_t * __restrict, const char * __restrict, int);
//Get data from cache
void *get_cache(struct cache *tree, char *name, size_t *len);
void extract_match(char * dest, const char * src, regmatch_t match);
//Free cache data
void *free_cache(struct cache * entry);

//Check if it is a get request
int check_request(char*);

//Get the url from the request
char * get_url(char *buf);

//Request data from the internet
void * request(char *hostname, char *req, char *url_name, size_t *len);

//Global Variables:
//Socket to client, root node of cache, mutext for cache.

int main(int argc , char *argv[])
{
    //Socket to client defined. Wait for incoming IPv4 and TCP connections...
    int client_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if(client_sock < 0)
    {
        printf("SOCKET CREATION FAILED");
        return 1;
    }

    //Client's Socket, Options, Flags.
    int options = 1, portno, flags;

    if(argc > 2)
    {
        printf("USAGE: <portno>\n");
        exit(0);
    }
    else
    {
        portno = atoi(argv[1]);
        //cache_timeout = atoi(argv[2]);
    }

    //Server and Client Address
    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));


    //Prepare the sockaddr_in structure
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = INADDR_ANY;
    sock_addr.sin_port = htons(portno);

    //Bind
    if(bind(client_sock, (struct sockaddr *)&sock_addr , sizeof(sock_addr)) < 0)
    {
        //print the error message
        perror("BIND FAILED");
        return 1;
    }
    printf("Bind done...\n");

//    if ((flags = fcntl(client_sock, F_GETFL, 0)) < 0)
//    {
//        return -1;
//    }
//    if (fcntl(client_sock, F_SETFL, flags | O_NONBLOCK) < 0)
//    {
//        return -1;
//    }

    //Listen
    listen(client_sock, LISTEN_COUNT);

    //Accept an incoming connection
    printf("Waiting for incoming connections...\n");

    int c = sizeof(struct sockaddr_in);

    pthread_mutex_t mutex_cache;
    pthread_mutex_init(&mutex_cache, 0);

    struct cache *cache = NULL;
    //While there is no sig int
    while(keep_running)
    {
        //Accept statement of client socket
        //socklen_t sock_len = sizeof(struct sock_addr_in);
        int client = accept(client_sock, (struct sockaddr *)&sock_addr, &c);

        if (client < 0)
        {
            perror("ACCEPT FAILED");
            return 1;
        }
        printf("Connection accepted...\n");

        //  retrieve the IP address of the client
        char ipBuffer[INET_ADDRSTRLEN];
        const char * ptr = inet_ntop(sock_addr.sin_family, &sock_addr.sin_addr, ipBuffer, sizeof(ipBuffer));

        //  DNS lookup to obtain the client hostname, if possible
        char hostname[255];
        if (!getnameinfo((struct sockaddr*)&sock_addr, sizeof(sock_addr), hostname, sizeof(hostname), 0, 0, NI_NAMEREQD))
        {
            printf("Incoming connection from %s [%s]\n", hostname, ipBuffer);
        }
        else
        {
            printf("Incoming connection from %s\n", ptr);
        }
        //Thread arguments with socket_desc and portno...
        struct thread_args *targs = malloc(sizeof(struct thread_args));
        targs->socket_desc = client;
        targs->mutex_cache = &mutex_cache;
        targs->root = cache;
        pthread_t sniffer_thread;

        if (pthread_create(&sniffer_thread, NULL, connection_handler, targs) != 0)
        {
            perror("COULD NOT CREATE CLIENT THREAD");
            return 1;
        }

        //Threads must be destroyed when thread exits.
        pthread_detach(sniffer_thread);
    }

    pthread_mutex_lock(&mutex_cache);
    free_cache(cache);
    cache = NULL;
    pthread_mutex_unlock(&mutex_cache);

    pthread_mutex_destroy(&mutex_cache);
    close(client_sock);
    return 0;
}

/*
 * This will handle connection for each client
 * */
void *connection_handler(void *input)
{
    //Error message for bad request
    char error_req[] = "HTTP 400 Bad Request\r\n";

    //Error for bad host name
    char error_host[] = "HTTP 404 Not Found\r\n";

    struct thread_args * targs = (struct thread_args *)input;
    printf("Socket Descriptor: %i\n", targs->socket_desc);

    //timeout values
    struct timeval timeout;
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;
    setsockopt(targs->socket_desc, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(targs->socket_desc, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    size_t position = recv(targs->socket_desc, buffer, sizeof(buffer) - 1, 0);
    if(position <= 0)
    {
        printf("RECV FAILED\n");
        close(targs->socket_desc);
        free(targs);
        return 0;
    }

//    if (check_request(buffer) != 0)
//    {
//        perror("NOT A GET REQUEST\n");
//        printf("%s\n", error_req);
//        send(targs->socket_desc, error_host, HOST_NAME_MAX, 0);
//        //exit(1);
//    }

    //  hit the cache for the resource
    size_t dataLength;
    void * data;

    char * req_url = get_url(buffer);

    //  we use a mutex to ensure threaded access is synchronized
    pthread_mutex_lock(targs->mutex_cache);
    data = get_cache(targs->root, req_url, &dataLength);
    pthread_mutex_unlock(targs->mutex_cache);

    if(data == NULL || dataLength == 0)
    {
        //Check host: If gethostbyname() returns NULL it isn't there
        //Also, gethostbyname() returns a pointer to a hostent structure
        //gethostname() works better because of how some etc/hosts are set up
        //https://stackoverflow.com/questions/2865583/gethostbyname-in-c
//        char * host_name;

        char *hst = get_hostname(buffer);
        if(hst == NULL)
        {
            printf("%s\n", error_host);
            send(targs->socket_desc, error_host, HOST_NAME_MAX, 0);
        }

        data = request(hst, buffer, req_url, &dataLength);
        if(data == NULL)
        {
            //send and close
            send(targs->socket_desc, error_req, strlen(error_req), 0);
            close(targs->socket_desc);
            free(targs);
            return NULL;
        }

        pthread_mutex_lock(targs->mutex_cache);

        printf("Caching resource: %s\n", req_url);

        struct cache * entry = cinit(req_url, data, dataLength);

        if(targs->root == NULL)
        {
            targs->root = entry;
        }
        else
        {
            append_cache(targs->root, entry);
        }

        pthread_mutex_unlock(targs->mutex_cache);

    }
    else
        printf("Cached resource: %s\n", req_url);

    write(targs->socket_desc, data, dataLength);
    close(targs->socket_desc);
    free(targs);
    return NULL;

}

void * request(char *hostname, char *req, char *url_name, size_t *len)
{
//    char *pos;
//    if ((pos=strchr(hostname, '\r')) != NULL)
//        *pos = '\0';

    struct addrinfo hints;
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *resolution = NULL;
    if(getaddrinfo(hostname, "80", &hints, &resolution) != 0)
    {
        printf("HOST NAME NOT FOUND");
        return NULL;
    }

    int new_sock = socket(resolution->ai_family, resolution->ai_socktype, resolution->ai_protocol);
    if (new_sock == 0)
    {
        printf("SOCKET CREATION ERROR");
        freeaddrinfo(resolution);
        return NULL;
    }


    if(connect(new_sock, resolution->ai_addr, resolution->ai_addrlen) != 0)
    {
        printf("UNABLE TO CONNECT TO: %s\n", hostname);
        close(new_sock);
        freeaddrinfo(resolution);
        return NULL;
    }

    if(write(new_sock, req, strlen(req)) != strlen(req))
    {
        printf("FAILED TO WRITE\n");
        close(new_sock);
        return NULL;
    }

    char read_buffer[1024];
    void *data = NULL;
    size_t position = 0;

    struct pollfd pfd;
    bzero(&pfd, sizeof(pfd));
    pfd.events = POLLIN | POLLRDNORM | POLLRDBAND;
    pfd.fd = new_sock;

    printf("Downloading content...\n");
    while(poll(&pfd, 1, 500) > 0)
    {
        ssize_t data_size = read(new_sock, read_buffer, sizeof(read_buffer));
        if(data_size > 0)
        {
            if(!data)
            {
                data = malloc((unsigned long)data_size);
            }
            else
            {
                data = realloc(data, position + data_size);
            }

            memcpy(&data[position], read_buffer, data_size);
            position += (size_t)data_size;
        }
        else
            break;

        close(new_sock);
        *len = data_size;
        return data;
    }
}

struct cache *cinit(char *name, void *data, size_t len)
{
    struct cache *c = malloc(sizeof(struct cache));
    c->data = data;
    c->length = len;
    c->left = NULL;
    c->right = NULL;

    c->url = name;

    return c;
}

void append_cache(struct cache *tree, struct cache *entry)
{
    if (tree == NULL || entry == NULL)
        return;

    if (strcmp(tree->url, entry->url) == 0)
    {
        if (tree->right != NULL)
        {
            append_cache(tree->right, entry);
        }
        else
        {
            tree->right = entry;
        }
    }
    else
    {
        if (tree->left != NULL)
        {
            append_cache(tree->left, entry);
        }
        else
        {
            tree->left = entry;
        }
    }
}

void *get_cache(struct cache *tree, char *name, size_t *len)
{
    //Go through each branch and see if there is a match
    if(tree == NULL)
    {
        *len = 0;
        return NULL;
    }

    //check current node and determine what to do:
    if (strcmp(tree->url, name) == 0)
    {
        //found it!
        *len = tree->length;
        return tree->data;
    }
    else if (strcmp(tree->url, name) > 0)
    {
        //go right
        return get_cache(tree->right, name, len);
    }

    //go left
    return get_cache(tree->left, name, len);
}

void *free_cache(struct cache * entry)
{
    if(entry == NULL)
        return NULL;
    free_cache(entry->left);
    free_cache(entry->right);
    free(entry->url);
    free(entry->data);
    free(entry);
}


/*
	check_request - simple function to check if the request is 
	GET reqeust. 
	Returns 0 upon success.
*/
int check_request(char* request)
{
	//make buffer for request to be in
	char *buffer = calloc(sizeof(char), 4);
	strcpy(buffer, request);
	
	//Check if it is a GET request return appropriate error codes.
	if(strcmp(buffer, "GET") != 0)
	{
		free(buffer);
		return -1;
	}
	else
	{
		free(buffer);
		return 0;
	}
}

char *get_url(char *buf)
{
    char *path  = NULL, *savePoint, *backup;

    if (buf == NULL || (backup = strdup(buf)) == NULL)
        return NULL;

    if(strtok_r(backup, " ", &savePoint))
    {
        path = strtok_r(NULL, " ", &savePoint);
        if(path)
            path=strdup(path);
    }

    return(path);
}

char *get_hostname(const char *buf)
{
    char *tmp = NULL, *path  = NULL, *savePoint = NULL, *backup = NULL, *c, *sp2;

    if (buf == NULL || (backup = strdup(buf)) == NULL)
        return NULL;
    strtok_r(backup, "\n", &savePoint);
    c = strtok_r(NULL, " ", &savePoint);
    if(c != NULL&& strcmp(c, "Host:") == 0) {
        c = strtok_r(NULL, "\n", &savePoint);
        path=strdup(c);
     }
    strtok_r(path, "\r", &savePoint);
    return(path);

}


