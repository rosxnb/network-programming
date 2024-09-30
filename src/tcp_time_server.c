#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <string.h>


int main()
{
    const char* PORT = "8080";
    printf("configuring local address on port %s...\n", PORT);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET6; // AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    struct addrinfo* bind_address;
    if (getaddrinfo(0, PORT, &hints, &bind_address) != 0)
    {
        printf("getaddrinfo() failed\n");
        return 1;
    }

    printf("creating socket...\n");
    int socket_listen;
    socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if (socket_listen < 0)
    {
        fprintf(stderr, "socket() failed. (%d)\n", errno);
        return 1;
    }

    // make support for dual-stack: IPV4 and IPV6
    int option = 0;
    if (setsockopt(socket_listen, IPPROTO_IPV6, IPV6_V6ONLY, (void*) &option, sizeof(option)) != 0)
    {
        fprintf(stderr, "setsockopt() failed. (%d)\n", errno);
        return 1;
    }

    printf("binding socket to local address...\n");
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen) != 0)
    {
        fprintf(stderr, "bind() failed. (%d)\n", errno);
        return 1;
    }
    freeaddrinfo(bind_address);

    printf("listening...\n");
    if (listen(socket_listen, 10) < 0)
    {
        fprintf(stderr, "listen() failed. (%d)\n", errno);
        return 1;
    }

    printf("waiting for connection...\n");
    struct sockaddr_storage client_address;
    socklen_t client_len = sizeof(client_address);
    int socket_client = accept(socket_listen, (struct sockaddr*) &client_address, &client_len);
    if (socket_client < 0)
    {
        fprintf(stderr, "accept() failed. (%d)\n", errno);
        return 1;
    }

    printf("client connected...\n");
    char address_buffer[100];
    getnameinfo((struct sockaddr*) &client_address, client_len, address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
    printf("%s\n", address_buffer);

    printf("reading request...\n");
    char request[1024];
    int bytes_received = recv(socket_client, request, 1024, 0);
    printf("received %d bytes.\n", bytes_received);
    printf("request:\n");
    printf("%.*s", bytes_received, request);

    printf("sending response...\n");
    const char* response = 
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: test/plain\r\n\r\n"
        "Local time is: ";
    int bytes_sent = send(socket_client, response, strlen(response), 0);
    printf("sent %d of %d bytes.\n", bytes_sent, (int)strlen(response));

    time_t timer;
    time(&timer);
    char* time_msg = ctime(&timer);
    bytes_sent = send(socket_client, time_msg, strlen(time_msg), 0);
    printf("sent %d of %d bytes.\n", bytes_sent, (int)strlen(time_msg));

    printf("closing connection...\n");
    close(socket_client);
    printf("closing listening socket...\n");
    close(socket_listen);
    printf("server shutdown successful.\n");

    return 0;
}
