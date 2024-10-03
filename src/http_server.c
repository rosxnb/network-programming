#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_REQUEST_SIZE 2048
#define PACKET_SIZE 1024


typedef struct ClientInfo
{
    int                     socket;
    int                     received;
    socklen_t               addressLength;
    char                    request[MAX_REQUEST_SIZE + 1];
    char                    addressBuffer[128];
    struct sockaddr_storage address;
    struct ClientInfo *     next;
} ClientInfo;

static ClientInfo * gpClients = 0;

ClientInfo *
getClient(int socket);

void
dropClient(ClientInfo * pClient);

const char *
getClientAddress(ClientInfo * pClient);

fd_set
waitOnClients(int socket);

void
send400(ClientInfo * pClient);

void
send404(ClientInfo * pClient);

void
serveResource(ClientInfo * pClient, const char * pPath);


void
reportErrorNExit(const char * pMsg);

const char *
getContentType(const char * pPath);

int
createSocket(const char * pHost, const char * pPort);


int main(void)
{
    int server = createSocket(0, "8080");

    while( 1 )
    {
        fd_set reads = waitOnClients(server);

        if( FD_ISSET(server, &reads) )
        {
            ClientInfo * pClient = getClient(-1);

            pClient->socket = accept( server, (struct sockaddr *) &(pClient->address), &(pClient->addressLength) );
            if( pClient->socket < 0 )
                reportErrorNExit("new client connection failed on accept");

            printf("New connection from %s\n", getClientAddress(pClient));
        }

        ClientInfo * pClient = gpClients;
        while( pClient )
        {
            ClientInfo * pNextClient = pClient->next;
            if( FD_ISSET(pClient->socket, &reads) )
            {
                if( pClient->received == MAX_REQUEST_SIZE )
                {
                    send400(pClient);
                    pClient = pNextClient;
                    continue;
                }

                int r = recv(pClient->socket, pClient->request + pClient->received, MAX_REQUEST_SIZE - pClient->received, 0);
                if( r < 1 )
                {
                    fprintf(stderr, "Connection lost with client: %s\n", getClientAddress(pClient));
                    dropClient(pClient);
                }
                else
                {
                    pClient->received                   += r;
                    pClient->request[pClient->received] = 0;

                    char * q = strstr(pClient->request, "\r\n\r\n");
                    if( q )
                    {
                        *q = 0;

                        if( strncmp("GET /", pClient->request, 5) )
                            send400(pClient);
                        else
                        {
                            char * path = pClient->request + 4;
                            char * endPath = strstr(path, " ");

                            if( !endPath )
                                send400(pClient);
                            else
                            {
                                *endPath = 0;
                                serveResource(pClient, path);
                            }
                        }
                    } // if( q )
                } // if( r < 1 )
            } // if( FD_ISSET(pClient->socket, &reads) )

            pClient = pNextClient;
        } // while( 1 )
    }

    printf("Closing server socket ... \n");
    close(server);
    printf("Finished.\n");

    return 0;
}


ClientInfo *
getClient(int socket)
{
    /*
       Find existing client or create one.
    */
    for( ClientInfo * pCurrentClient = gpClients; pCurrentClient; pCurrentClient = pCurrentClient->next )
        if( pCurrentClient->socket == socket ) return pCurrentClient;

    ClientInfo * newClient = (ClientInfo *) calloc(1, sizeof(ClientInfo));
    if( !newClient ) reportErrorNExit("malloc");

    newClient->addressLength = sizeof(newClient->address);
    newClient->next          = gpClients;

    gpClients = newClient;
    return newClient;
}

void
dropClient(ClientInfo * pClient)
{
    /*
       Deletes specified client else report error and exit if not found.
    */
    for( ClientInfo ** ppCurrent = &gpClients; *ppCurrent; ppCurrent = &(*ppCurrent)->next )
        if( *ppCurrent == pClient )
        {
            *ppCurrent = pClient->next;
            close(pClient->socket);
            free(pClient);
            return;
        }

    fprintf(stderr, "dropClient() got invalid argmuent.\n");
    exit(1);
}

const char *
getClientAddress(ClientInfo * pClient)
{
    getnameinfo((struct sockaddr *) &pClient->address, pClient->addressLength, pClient->addressBuffer, sizeof(pClient->addressBuffer), 0, 0, NI_NUMERICHOST);
    return pClient->addressBuffer;
}

fd_set
waitOnClients(int socket)
{
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(socket, &reads);

    int maxSocket = socket;
    ClientInfo * pClient = gpClients;

    while( pClient )
    {
        FD_SET(pClient->socket, &reads);
        if( maxSocket < pClient->socket ) maxSocket = pClient->socket;
        pClient = pClient->next;
    }

    if( select(maxSocket + 1, &reads, 0, 0, 0) < 0 )
        reportErrorNExit("select");

    return reads;
}

void
send400(ClientInfo * pClient)
{
    const char * pResponseMsg = 
        "HTTP/1.1 400 Bad Request \r\n"
        "Connection: close\r\n"
        "Content-Length: 11\r\n\r\n"
        "Bad Request";

    send(pClient->socket, pResponseMsg, strlen(pResponseMsg), 0);
    dropClient(pClient);
}

void
send404(ClientInfo * pClient)
{
    const char * pResponseMsg = 
        "HTTP/1.1 404 Not Found\r\n"
        "Connection: close\r\n"
        "Content-Length: 9\r\n\r\n"
        "Not Found";

    send(pClient->socket, pResponseMsg, strlen(pResponseMsg), 0);
    dropClient(pClient);
}

void
serveResource(ClientInfo * pClient, const char * pPath)
{
    printf("serveResource( %s, %s )\n", getClientAddress(pClient), pPath);

    if( strcmp(pPath, "/") == 0 )   { pPath = "/index.html"; }
    if( strlen(pPath) > 100 )            { send400(pClient); return; }
    if( strstr(pPath, "..") )  { send404(pClient); return; }

    char fullPath[128];
    sprintf(fullPath, "../src/public%s", pPath); // assumes executable is in build/ dir

    FILE *pFile = fopen(fullPath, "rb");
    if( !pFile )
    {
        send404(pClient);
        return;
    }

    fseek(pFile, 0L, SEEK_END);
    size_t fileSize = ftell(pFile);
    rewind(pFile);

    const char * fileType = getContentType(fullPath);
    char buffer[PACKET_SIZE];

    sprintf(buffer, "HTTP/1.1 200 OK\r\n");
    send(pClient->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Connection: close\r\n");
    send(pClient->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Length: %lu\r\n", fileSize);
    send(pClient->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Type: %s\r\n", fileType);
    send(pClient->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "\r\n\r\n");
    send(pClient->socket, buffer, strlen(buffer), 0);

    int r = fread(buffer, 1, PACKET_SIZE, pFile);
    while( r )
    {
        send(pClient->socket, buffer, r, 0);
        r = fread(buffer, 1, PACKET_SIZE, pFile);
    }

    fclose(pFile);
    dropClient(pClient);
}


const char *
getContentType(const char * pPath)
{
    const char * pLastDot = strrchr(pPath, '.');
    if( pLastDot )
    {
        if( strcmp(pLastDot, ".css")  == 0) return "text/css";
        if( strcmp(pLastDot, ".csv")  == 0) return "text/csv";
        if( strcmp(pLastDot, ".gif")  == 0) return "image/gif";
        if( strcmp(pLastDot, ".htm")  == 0) return "text/html";
        if( strcmp(pLastDot, ".html") == 0) return "text/html";
        if( strcmp(pLastDot, ".ico")  == 0) return "image/x-icon";
        if( strcmp(pLastDot, ".jpeg") == 0) return "image/jpeg";
        if( strcmp(pLastDot, ".jpg")  == 0) return "image/jpeg";
        if( strcmp(pLastDot, ".js")   == 0) return "application/javascript";
        if( strcmp(pLastDot, ".json") == 0) return "application/json";
        if( strcmp(pLastDot, ".png")  == 0) return "image/png";
        if( strcmp(pLastDot, ".pdf")  == 0) return "application/pdf";
        if( strcmp(pLastDot, ".svg")  == 0) return "image/svg+xml";
        if( strcmp(pLastDot, ".txt")  == 0) return "text/plain";
    }
    return "application/octet-stream";
}


int
createSocket(const char * pHost, const char * pPort)
{
    printf("Configuring local address ... \n");
    struct addrinfo hints;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family   = AF_INET;
    hints.ai_flags    = AI_PASSIVE;

    struct addrinfo * pLocalAddr;
    if( getaddrinfo(pHost, pPort, &hints, &pLocalAddr) ) reportErrorNExit("getaddrinfo");

    printf("Creating socket ... \n");
    int s = socket( pLocalAddr->ai_family, pLocalAddr->ai_socktype, pLocalAddr->ai_protocol );
    if( s < 0 ) reportErrorNExit("socket");

    printf("Binding socket to local address ... \n");
    if( bind(s, pLocalAddr->ai_addr, pLocalAddr->ai_addrlen) ) reportErrorNExit("bind");
    freeaddrinfo(pLocalAddr);

    printf("Activating LISTEN mode on created socket ... \n");
    if( listen(s, 10) < 0 ) reportErrorNExit("listen");

    return s;
}


void
reportErrorNExit(const char * pMsg)
{
    fprintf(stderr, "%s() failed. errno %d \n", pMsg, errno);
    exit(1);
}
