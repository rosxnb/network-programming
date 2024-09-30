#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

const unsigned char * 
printName(const unsigned char * msg, const unsigned char * pName, const unsigned char * end);

void
printDNSMessage(const char * msg, const int msgLen);

int main(int argc, char * argv[])
{
    /*
        char dns_query[] = {
            0xAB,   0xCD,         // ID
            0x01,   0x00,         // Recursion
            0x00,   0x01,         // QDCOUNT
            0x00,   0x00,         // ANCOUNT
            0x00,   0x00,         // NSCOUNT
            0x00, 0x00,         // ARCOUNT
            7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', // label
            3, 'c', 'o', 'm',                     // label
            0,                                    // End of label
            0x00, 0x01,         // QTYPE = A
            0x00, 0x01,         // QCLASS
        };
    */

    if( argc < 3 )
    {
        printf("Usage: dns_query <hostname> <type>\n");
        printf("<type> can be: \n");
        printf("\t\t- a\n");
        printf("\t\t- aaaa\n");
        printf("\t\t- txt\n");
        printf("\t\t- mx\n");
        printf("\t\t- any\n");
        return 1;
    }

    if( strlen(argv[1]) > 253 )
    {
        perror("Error: hostname too long\n");
        return 1;
    }

    unsigned char type;
    if     ( strcmp(argv[2], "a") == 0 )    type = 1;
    else if( strcmp(argv[2], "mx") == 0 )   type = 15;
    else if( strcmp(argv[2], "txt") == 0 )  type = 16;
    else if( strcmp(argv[2], "aaaa") == 0 ) type = 28;
    else if( strcmp(argv[2], "any") == 0 )  type = 255;
    else { perror("Unknown <type> provided.\n"); return 1; }

    printf("Configuring local address ...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    
    struct addrinfo * localAddr;
    if( getaddrinfo("8.8.8.8" /*Public DNS Server*/, "53" /*UDP port for DNS*/, &hints, &localAddr) )
    {
        fprintf(stderr, "getaddrinfo() failed. errno: %d\n", errno);
        return 1;
    }

    printf("Creating socket ...\n");
    int socketPeer = socket(localAddr->ai_family, localAddr->ai_socktype, localAddr->ai_protocol);
    if( socketPeer < 0 )
    {
        fprintf(stderr, "socket() failed. errno: %d\n", errno);
        return 1;
    }

    char dnsQuery[1024] = {
        0xAB,   0xCD,  // ID
        0x01,   0x00,  // Recursion
        0x00,   0x01,  // QDCOUNT
        0x00,   0x00,  // ANCOUNT
        0x00,   0x00,  // NSCOUNT
        0x00, 0x00,  // ARCOUNT
    };

    // encode user desired hosname into dnsQuery
    char * p = dnsQuery + 12;
    char * h = argv[1];
    while( *h )
    {
        char * len = p;
        p++; // leave vacant space for storing count
        if( h != argv[1] ) ++h; // skip the '.' char for subsequent loop invariant

        while( *h && *h != '.' ) *p++ = *h++;
        *len = p - len - 1; // fill vacant space for storing count
    }
    *p++ = 0; // End Label byte

    // add question type and question class
    *p++ = 0x00; *p++ = type; // QTYPE
    *p++ = 0x00; *p++ = 0x01; // QCLASS

    // compute dnsQuery len
    const int queryLen = p - dnsQuery;


    int bytesSent = sendto(socketPeer, dnsQuery, queryLen, 0, localAddr->ai_addr, localAddr->ai_addrlen);
    printf("Sent %d out of %d message.\n", bytesSent, queryLen);
    printDNSMessage(dnsQuery, queryLen);
    printf("\n");

    char read[1024];
    int bytesReceived = recvfrom(socketPeer, read, 1024, 0, 0, 0);
    printf("Received %d bytes.\n", bytesReceived);
    printDNSMessage(read, bytesReceived);
    printf("\n");

    close(socketPeer);

    return 0;
}

const unsigned char * 
printName(const unsigned char * msg, const unsigned char * pName, const unsigned char * end)
{
    if( pName + 2 > end )
    {
        perror("End of message.\n");
        exit(1);
    }

    // check if p points to pointer
    if( (*pName & 0xC0) == 0xC0) // 0xC0 = 0b11000000
    {
        const int k = ((*pName & 0x0b00111111) << 8) + pName[1];
        pName += 2;
        printf(" (pointer %d) ", k);
        printName(msg, msg+k, end);
        return pName;
    }

    const int len = *pName++;
    if( pName + len + 1 > end )
    {
        perror("End of message.\n");
        exit(1);
    }

    printf("%.*s", len, pName);
    pName += len;

    if( *pName )
    {
        printf(".");
        return printName(msg, pName, end);
    }
    else return pName + 1;
}

void
printDNSMessage(const char * msg, const int msgLen)
{
    if ( msgLen < 12 )
    {
        perror("Message is too short to be valid.\n");
        exit(1);
    }

    const unsigned char * message = (const unsigned char*) msg;

    // print raw DNS message
    for( int i = 0; i < msgLen; ++i )
    {
        unsigned char r = msg[i];
        printf("%02d:   %02X   %03d   '%c'\n", i, r, r, r);
    } printf("\n");

    printf("ID = %0X %0X\n", message[0], message[1]);

    const int qr = (message[2] & 0x80) >> 7; // check if MSB is set
    printf("QR = %d %s\n", qr, qr ? "response" : "query");

    const int opcode = (message[2] & 0x78) >> 3;
    printf("OPCODE = %d ", opcode);
    switch( opcode )
    {
        case 0:  printf("standard \n"); break;
        case 1:  printf("reverse \n"); break;
        case 2:  printf("status \n"); break;
        default: printf("? \n");
    }

    const int aa = (message[2] & 0x04) >> 2;
    printf("AA = %d %s\n", aa, aa ? "authoritative" : "");

    const int tc = (message[2] & 0x02) >> 1;
    printf("TC = %d %s\n", tc, tc ? "message truncated" : "");

    const int rd = (message[2] & 0x01);
    printf("RD = %d %s\n", rd, rd ? "recursion desired" : "");

    if( qr )
    {
        const int rcode = msg[3] & 0x07;
        printf("RCODE = %d ", rcode);
        switch(rcode)
        {
            case 0:   printf("sucess\n"); break;
            case 1:   printf("format error\n"); break;
            case 2:   printf("server failure\n"); break;
            case 3:   printf("name error\n"); break;
            case 4:   printf("not implemented\n"); break;
            case 5:   printf("refused\n"); break;
            default:  printf("?\n");
        }
        if( rcode != 0 ) return;
    }

    const int qdcount = (message[4]  << 8) + message[5];
    const int ancount = (message[6]  << 8) + message[7];
    const int nscount = (message[8]  << 8) + message[9];
    const int arcount = (message[10] << 8) + message[11];

    printf("QDCOUNT = %d\n", qdcount);
    printf("ANCOUNT = %d\n", ancount);
    printf("NSCOUNT = %d\n", nscount);
    printf("ARCOUNT = %d\n", arcount);

    const unsigned char * p   = message + 12;
    const unsigned char * end = message + msgLen;

    if( qdcount )
    {
        for( int i = 0; i < qdcount; ++i )
        {
            if( p >= end )
            {
                perror("End of message.\n");
                exit(1);
            }

            printf("Query %2d\n", i + 1);
            printf("  name: ");
            p = printName(message, p, end); printf("\n");

            if( p + 4 > end )
            {
                perror("End of message.\n");
                exit(1);
            }

            const int qtype = (p[0] << 8) + p[1];
            printf("  type: %d\n", qtype);
            p += 2;

            const int qclass = (p[0] << 8) + p[1];
            printf("  class: %d\n", qclass);
            p += 2;
        }
    }

    if( ancount || nscount || arcount )
    {
        for( int i = 0; i < ancount + nscount + arcount; ++i )
        {
            if( p >= end )
            {
                perror("End of message.\n");
                exit(1);
            }

            printf("Answer %2d\n", i + 1);
            printf("  name: ");
            p = printName(message, p, end); printf("\n");

            if( p + 10 > end )
            {
                perror("End of message.\n");
                exit(1);
            }

            const int type = (p[0] << 8) + p[1];
            printf("  type: %d\n", type);
            p += 2;

            const int class = (p[0] << 8) + p[1];
            printf("  class: %d\n", class);
            p += 2;

            const unsigned int ttl = (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
            printf("  ttl: %u\n", ttl);
            p += 4;

            const int rdlen = (p[0] << 8) + p[1];
            printf("  rdlen: %d\n", rdlen);
            p += 2;

            if( p + rdlen > end)
            {
                perror("End of message.\n");
                exit(1);
            }

            if( rdlen == 4 && type == 1 ) // A record
            {
                printf("Address ");
                printf("%d.%d.%d.%d\n", p[0], p[1], p[2], p[3]);
            }
            else if( type == 15  && rdlen > 3 ) // MX Record
            {
                const int preference = (p[0] << 8) + p[1];
                printf("  pref: %d\n", preference);
                printf("MX: ");
                printName(message, p+2, end);
                printf("\n");
            }
            else if( rdlen == 16 && type == 28 ) // AAAA Record
            {
                printf("Address ");
                for( int j = 0; j < rdlen; ++j )
                {
                    printf("%02x%02x", p[j], p[j+1]);
                    if( j + 2 < rdlen )
                        printf(":");
                }
                printf("\n");
            }
            else if( type == 16 ) // TXT Record
            {
                printf("TXT: %*.s\n", rdlen - 1, p + 1);
            }
            else if( type == 5) // CNAME Record
            {
                printf("CNAME: ");
                printName(message, p, end);
                printf("\n");
            }

            p += rdlen;
        }
    }

    if( p != end ) printf("There are some unread data leftover still.\n");
    printf("\n");
    return ;
}
