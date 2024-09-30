#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdio.h>
#include <string.h>


int main(int argc, char* argv[])
{
    if( argc < 3 )
    {
        fprintf(stderr, "Useage: tcp_client hostname port\n");
        return 1;
    }

    fprintf(stdout, "Configuring remote address ...\n");

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* peer_address;
    if( getaddrinfo(argv[1], argv[2], &hints, &peer_address) )
    {
        fprintf(stderr, "getaddrinfo() failed with errorno: %d\n", errno);
        return 1;
    }

    fprintf(stdout, "Remote address is: ");
    char address_buffer[100], service_buffer[100];
    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen, address_buffer, sizeof(address_buffer), service_buffer, sizeof(service_buffer), NI_NUMERICHOST);
    fprintf(stdout, "%s %s\n", address_buffer, service_buffer);

    fprintf(stdout, "Creating socket ...\n");
    int socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype, peer_address->ai_protocol);
    if( socket_peer < 0 )
    {
        fprintf(stderr, "Unable to create socket. Error code: %d\n", errno);
        return 1;
    }

    fprintf(stdout, "Connecting to server ... \n");
    if( connect(socket_peer, peer_address->ai_addr, peer_address->ai_addrlen) )
    {
        fprintf(stderr, "connect() failed with errno: %d\n", errno);
        return 1;
    }
    freeaddrinfo(peer_address);

    fprintf(stdout, "Connection to server established.\nTo send data, enter text followed by <Enter>.\n");
    while( 1 )
    {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(socket_peer, &reads);
        FD_SET(0, &reads); // for terminal inputs

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        if( select(socket_peer + 1, &reads, 0, 0, &timeout) < 0 )
        {
            fprintf(stderr, "select() failed with errno: %d\n", errno);
            return 1;
        }

        if( FD_ISSET(socket_peer, &reads) )
        {
            char read[4096];
            int bytes_received = recv(socket_peer, read, 4096, 0);
            if( bytes_received < 1 )
            {
                fprintf(stdout, "Connection closed by peer.\n");
                break;
            }
            fprintf(stdout, "Received (%d) bytes: %.*s\n", bytes_received, bytes_received, read);
        }

        if( FD_ISSET(0, &reads) )
        {
            char read[4096];
            if( !fgets(read, 4096, stdin) ) break;
            fprintf(stdout, "Sending data: %s\n", read);
            int bytes_sent = send(socket_peer, read, strlen(read), 0);
            fprintf(stdout, "Sent %d bytes.\n", bytes_sent);
        }
    }

    printf("Closing socket ...\n");
    close(socket_peer);
    printf("Finished.\n");
    return 0;
}
