#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define TIMEOUT 5.0

void parseURL       (char * url, char ** hostname, char ** port, char ** path);
void sendRequest    (int socket, char * hostname, char * port, char * path);
int connectToHost   (char * hostname, char * port);


int main(int argc, char * argv[])
{
    if( argc < 2 )
    {
        perror("Usage: http_client <url>\n");
        return 1;
    }

    char * url = argv[1];
    char * hostname;
    char * port;
    char * path;
    parseURL(url, &hostname, &port, &path);

    int server = connectToHost(hostname, port);
    sendRequest(server, hostname, port, path);

    const clock_t startTime = clock();

// #define RESPONSE_SIZE 8192 // 8 MB
#define RESPONSE_SIZE 51200 // 50 MB
    // char response[RESPONSE_SIZE+1];
    char * response = (char *) malloc(sizeof(char) * RESPONSE_SIZE);
    if( !response )
    {
        fprintf(stderr, "malloc() failed. errno: %d\n", errno);
        return 1;
    }
    char * p = response;
    char * q;
    char * end = response + RESPONSE_SIZE;
    char * body = 0;

    enum{ length, chunked, connection }; // ways reponse body length can be determined
    int encoding = 0;
    int remaining = 0;

    while( 1 )
    {
        if( 1.f * (clock() - startTime) / CLOCKS_PER_SEC > TIMEOUT )
        {
            fprintf(stderr, "timeout after %.2f seconds\n", TIMEOUT);
            return 1;
        }
        
        if( p == end )
        {
            fprintf(stderr, "out of buffer space\n");
            return 1;
        }

        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(server, &reads);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;

        if( select(server + 1, &reads, 0, 0, &timeout) < 0)
        {
            fprintf(stderr, "select() failed. errno: %d\n", errno);
            return 1;
        }

        if( FD_ISSET(server, &reads) )
        {
            int bytesReceived = recv(server, p, end - p, 0);
            if( bytesReceived < 1 )
            {
                if( encoding == connection && body )
                {
                    printf("%.*s", (int)(end - body), body);
                }
                printf("\nConnection closed by peer.\n");
                break;
            }

            /*
               printf("Received %d bytes: %.*s", bytesReceived, bytesReceived, p);
            */

            p += bytesReceived;
            *p = 0;

            if( !body && (body = strstr(response, "\r\n\r\n")) )
            {
                *body = 0;
                body += 4;

                printf("Received Headers: \n%s\n\n", response);

                q = strstr(response, "\nContent-Length: ");
                if( q )
                {
                    encoding = length;
                    q = strchr(q, ' ');
                    ++q;
                    remaining = strtol(q, 0, 10);
                }
                else
                {
                    q = strstr(response, "Transfer-Encoding: chunked");
                    if( q )
                    {
                        encoding = chunked;
                        remaining = 0;
                    }
                    else encoding = connection;
                }

                printf("Received Body: \n");
            }

            if( body )
            {
                if( encoding == length )
                {
                    if( p - body >= remaining)
                    {
                        printf("%.*s\n", remaining, body);
                        break;
                    }
                }
                else if( encoding == chunked )
                {
                    do
                    {
                        if( remaining == 0 )
                        {
                            if( (q = strstr(body, "\r\n\r\n")) )
                            {
                                remaining = strtol(body, 0, 16);
                                if( !remaining ) goto finish;
                                body = q + 2;
                            }
                            else break;
                        }
                        
                        if( remaining && p - body >= remaining )
                        {
                            printf("%.*s\n", remaining, body);
                            body += remaining + 2;
                            remaining = 0;
                        }
                    } while( !remaining );
                }
            }
        }
    }

finish:
    free(response);
    printf("Closing socket ...\n");
    close(server);
    printf("Finished. \n");

    return 0;
}

int connectToHost(char * hostname, char * port)
{
    printf("Configuring remote address ...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo * peerAddr;
    if( getaddrinfo(hostname, port, &hints, &peerAddr) )
    {
        fprintf(stderr, "getaddrinfo() faile. errno: %d\n", errno);
        exit(1);
    }

    char addrBuffer[100], serviceBuffer[100];
    getnameinfo(peerAddr->ai_addr, peerAddr->ai_addrlen, addrBuffer, sizeof(addrBuffer), serviceBuffer, sizeof(serviceBuffer), NI_NUMERICHOST);
    printf("Remote address: %s\n", addrBuffer);
    printf("Service name: %s\n\n", serviceBuffer);

    printf("Creating socket ...\n");
    int s = socket(peerAddr->ai_family, peerAddr->ai_socktype, peerAddr->ai_protocol);
    if( s < 0 )
    {
        fprintf(stderr, "socket() failed. errno: %d\n", errno);
        exit(1);
    }

    printf("Connecting to server ...\n");
    if( connect(s, peerAddr->ai_addr, peerAddr->ai_addrlen) )
    {
        fprintf(stderr, "connect() failed. errno: %d\n", errno);
        exit(1);
    }
    freeaddrinfo(peerAddr);
    printf("Connected to server.\n\n");

    return s;
}

void sendRequest(int socket, char * hostname, char * port, char * path)
{
    char buffer[2048];

    sprintf(buffer, "GET /%s HTTP/1.1\r\n", path);
    sprintf(buffer + strlen(buffer), "Host: %s:%s\r\n", hostname, port);
    sprintf(buffer + strlen(buffer), "Connection: close\r\n");
    sprintf(buffer + strlen(buffer), "User-Agent: honpwc web_get 1.0\r\n");
    sprintf(buffer + strlen(buffer), "\r\n");

    send(socket, buffer, strlen(buffer), 0);
    printf("Sent Headers: \n%s", buffer);
}

void parseURL(char * url, char ** hostname, char ** port, char ** path)
{
    char * p        = strstr(url, "://");
    char * protocol = 0;

    if( p )
    {
        protocol = url;
        *p = 0; // null terminate the protocol string
        p += 3; // move past "://" characters
    }
    else
        p = url;

    if( protocol )
    {
        if( strcmp(protocol, "http") )
        {
            fprintf(stderr, "Only supports 'http' protocol but provided %s\n", protocol);
            exit(0);
        }
    }

    *hostname = p;
    while( *p && *p != ':' && *p != '/' && *p != '#' ) ++p;

    *port = "80";
    if( *p == ':' )
    {
        *p++ = 0;
        *port = p;
    }
    while( *p && *p != '/' && *p != '#' ) ++p;

    *path = p;
    if( *p == '/' ) *path = p + 1;
    *p = 0;

    ++p;
    while( *p && *p != '#') ++p;
    if( *p == '#' ) *p = 0;

    printf("URL parts: \n");
    printf("\thostname: %s\n", *hostname);
    printf("\tport: %s\n", *port);
    printf("\tpath: %s\n\n", *path);
}
