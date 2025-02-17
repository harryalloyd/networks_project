/* This code is an updated version of the sample code from "Computer Networks: A Systems
 * Approach," 5th Edition by Larry L. Peterson and Bruce S. Davis. Some code comes from
 * man pages, mostly getaddrinfo(3). */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

// #define SERVER_PORT "5432" --> will likely get rid of this because I declared port in main
#define MAX_LINE 1000

/*
 * Lookup a host IP address and connect to it using service. Arguments match the first two
 * arguments to getaddrinfo(3).
 *
 * Returns a connected socket descriptor or -1 on error. Caller is responsible for closing
 * the returned socket.
 */
int lookup_and_connect( const char *host, const char *service );
int sendall(int s, char *buf, int *len);
int recvAll(int s, char *buf, int *len);


int main( int argc, char *argv[] ) {
	const char *http_request = "GET /~kkredo/file.html HTTP/1.0\r\n\r\n";
    const char *host = "www.ecst.csuchico.com";
    const char *port = "80";

	int chunk_size;
	int h1_tags_count = 0;
	int total_bytes = 0;
	int s;
    char* position=0;
    int total_received =0;

	// not positive how these will be used yet. Might get rid of it 
	char buf[MAX_LINE];
	int len;

	// Here we're checking to make sure there are the correct # of command-line arguments
	// The program is expecting two arguments, the first being the name of the program, i.e.
	// ./counter-h1 and the second argument being the chunk size, i.e. 650
	if ( argc == 2 ) {
		host = argv[1];
	}
	else {
		fprintf( stderr, "usage: %s host\n", argv[0] );
		exit( 1 );
	}

	/* Lookup IP and connect to server */
	if ( ( s = lookup_and_connect( host, port ) ) < 0 ) {
		exit( 1 );
	}

	// We need to grab the size of the chunk_size that the user enters
	chunk_size = atoi( argv[1] );

	// Checking to make sure the user enter a valid chunk size between 1 and 1000
	if ( chunk_size > 1000 || chunk_size < 1) {
		fprintf(stderr, "You entered the chunk size %d make sure its between 1-1000\n", chunk_size);
		exit ( 1 );
	}

    //may need to change this to chunk size
    len = strlen(http_request);


	//Think of partial sends.... and don't forget to check for an error and close socket
    if(sendAll(s, http_request, len, 0) < 0) {
        perror("sendall failed");
        close(s);
        return -1;
    }


	while (1) {                   // Think of partial recieves and don't forget to check for an error and close socket
        //this is where we'll extract the data                  While recieving data strstr() search for "<H1>" up til chunk size defined by command line. then increment counter and position pointer by 4

        len = sizeof(buf) - total_received;
        int bytes_received = recvAll(s, buf + total_received, len);
        if(bytes_received < 0){
        perror("recv failed");
        close (s);
        return -1
        }
        if(bytes_received ==0) {
            break;
        }
        total_received += bytes_received;
    }

	// After the while loop grabs all the data we'll print below
	printf("Number of <h1> tags: %d\n", h1_tags_count);
	printf("Number of bytes: %d\n", total_bytes);

	close( s );
	return 0;
}


int sendAll(int s, char *buf, int *len) {
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;
    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    *len = total; // return number actually sent here
    return n==-1?-1:0; // return -1 on failure, 0 on success
}

int recvAll(int s, char *buf, int *len) {
    int total=0;
    int bytesleft = *len;
    int n;
    while(total < *len){
        n = recv(s, buf+total, bytesleft,0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    *len = total; // return number actually sent here
    return n==-1?-1:0; // return -1 on failure, 0 on success
    }
}


int lookup_and_connect( const char *host, const char *service ) {
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s;

	/* Translate host name into peer's IP address */
	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	if ( ( s = getaddrinfo( host, service, &hints, &result ) ) != 0 ) {
		fprintf( stderr, "stream-talk-client: getaddrinfo: %s\n", gai_strerror( s ) );
		return -1;
	}

	/* Iterate through the address list and try to connect */
	for ( rp = result; rp != NULL; rp = rp->ai_next ) {
		if ( ( s = socket( rp->ai_family, rp->ai_socktype, rp->ai_protocol ) ) == -1 ) {
			continue;
		}

		if ( connect( s, rp->ai_addr, rp->ai_addrlen ) != -1 ) {
			break;
		}

		close( s );
	}
	if ( rp == NULL ) {
		perror( "stream-talk-client: connect" );
		return -1;
	}
	freeaddrinfo( result );

	return s;
}
