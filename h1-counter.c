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


/*
 * Lookup a host IP address and connect to it using service. Arguments match the first two
 * arguments to getaddrinfo(3).
 *
 * Returns a connected socket descriptor or -1 on error. Caller is responsible for closing
 * the returned socket.
 */
int lookup_and_connect( const char *host, const char *service );

int send_data_to_soc( int s, const char *buf, int *len );

int recv_data_from_soc( int s, char *buf, int *len );


int main( int argc, char *argv[] ) {
	const char *http_request = "GET /~kkredo/file.html HTTP/1.0\r\n\r\n";
    const char *host = "www.ecst.csuchico.edu";
    const char *port = "80";

	int chunk_size;
	int h1_tags_count = 0;
	int total_bytes = 0;
	char *bufs = NULL;
	int s;
	int len;
	


	
	if (argc != 2) { // we're checking to make sure we get the correct amount of arguments
		perror("Incorrect number of arguments given");
        exit(1);
    }

	/* Lookup IP and connect to server */
	if ( ( s = lookup_and_connect( host, port ) ) < 0 ) {
		perror("Failed to connect");
		exit( 1 );
	}

	// We need to grab the size of the chunk_size that the user enters to make sure  its valid
	chunk_size = atoi( argv[1] );

	// Checking to make sure the user enter a valid chunk size between 1 and 1000
	if ( chunk_size > 1000 || chunk_size < 1) {
		perror("chucnk size must be between 1 and 1000");
		exit ( 1 );
	}

    //may need to change this to chunk size
    len = (int)strlen(http_request);


    if (send_data_to_soc(s, http_request, &len) < 0) {
        perror("send_data_to_soc failed");
        close(s);
        return -1;
    }

	bufs = malloc(chunk_size + 1); // +1 to account for the null terminator
    if (!bufs) {
		perror("Memory allocation for bufs failed");
        close(s);
        return -1;
    }


	while (1) {                   
        int byte_from_request = chunk_size; //we want the chunk_size bytes for every request
        int result = recv_data_from_soc(s, bufs, &byte_from_request); //here we want to read that many bytes that came in

        if (result < 0) {
            perror("recvAll");
            break;
        }
        if (byte_from_request == 0) { // when there's no more data
            break;
        }

        bufs[byte_from_request] = '\0';  // so we won't read past the data
        total_bytes += byte_from_request;
        char *h1_tag_search = bufs; //where we search for h1 tags in the new 'chunk_size' chunk of data 
        while ((h1_tag_search = strstr(h1_tag_search, "<h1>")) != NULL) {
            h1_tags_count++;
            h1_tag_search += 4; 
        }
    }

	// After the while loop grabs all the data we'll print below
	printf("Number of <h1> tags: %d\n", h1_tags_count);
	printf("Number of bytes: %d\n", total_bytes);

	close( s );
	return 0;
}


int send_data_to_soc(int s, const char *buf, int *len) {
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;
    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { 
			break; 
		}
        total += n;
        bytesleft -= n;
    }
    *len = total; // return number actually sent here
    return (n == -1) ? -1 : 0; // here we'll return -1 if failure, 0 if good
}

int recv_data_from_soc(int s, char *buf, int *len) {
    int total=0;
    int bytesleft = *len;
    int n;

    while(total < *len){
        n = recv(s, buf + total, bytesleft,0);
        if (n <= 0) { 
			break; 
		}
        total += n;
        bytesleft -= n;
    }
    *len = total; // return number actually sent here
    return (n < 0) ? -1 : 0; // return -1 on failure, 0 on success
}


// we shouldn't need to change any of  this 
int lookup_and_connect( const char *host, const char *service ) {
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s;

	/* Translate host name into peer's IP address */
	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
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
