/******************************************************************************
 * EECE 446 - Program 1
 * HTML Tag and Byte Counter
 * Spring 2025
 *
 * Group Members:
 *   - Your Names Here
 *
 * This program connects to www.ecst.csuchico.edu on port 80 and requests the
 * file /~kkredo/file.html. It reads the file in fixed-size chunks. For each
 * chunk, it counts the number of <h1> tags (ignoring any that may span chunk
 * boundaries) and sums all bytes received. It then prints the final counts.
 *
 * Derived in part from sample code in "Computer Networks: A Systems Approach,"
 * Peterson & Davie, 5th Ed., as well as sample code provided in class.
 ******************************************************************************/

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <sys/types.h>
 #include <sys/socket.h>
 #include <netdb.h>
 
 /* Forward declarations */
 int lookup_and_connect(const char *host, const char *service);
 int send_all(int sockfd, const char *buf, int len);
 int recv_chunk_exact(int sockfd, char *buf, int chunk_size);
 
 /******************************************************************************
  * main
  ******************************************************************************/
 int main(int argc, char *argv[])
 {
     /* Local string constants for host, port, and the HTTP GET request */
     const char *host         = "www.ecst.csuchico.edu";
     const char *port         = "80";
     const char *http_request = "GET /~kkredo/file.html HTTP/1.0\r\n\r\n";
 
     int s;                    /* Socket descriptor */
     int chunk_size;           /* Number of bytes to read at a time */
     int h1_count = 0;         /* Number of <h1> tags found */
     int total_bytes = 0;      /* Total bytes received */
     char *chunk_buf = NULL;   /* Buffer for each chunk of data */
 
     /* Check usage */
     if (argc != 2) {
         fprintf(stderr, "usage: %s chunk_size\n", argv[0]);
         return 1;
     }
 
     /* Parse chunk_size from command line */
     chunk_size = atoi(argv[1]);
     if (chunk_size < 1 || chunk_size > 1000) {
         fprintf(stderr, "Error: chunk_size must be in [1..1000]\n");
         return 1;
     }
 
     /* 1. Connect to the specified host and port */
     if ((s = lookup_and_connect(host, port)) < 0) {
         return 1;
     }
 
     /* 2. Send the HTTP GET request (handle partial sends) */
     if (send_all(s, http_request, strlen(http_request)) < 0) {
         perror("send");
         close(s);
         return 1;
     }
 
     /* 3. Allocate the chunk buffer (plus one byte for '\0' terminator) */
     chunk_buf = malloc(chunk_size + 1);
     if (!chunk_buf) {
         fprintf(stderr, "Failed to allocate chunk buffer.\n");
         close(s);
         return 1;
     }
 
     /* 4. Repeatedly receive fixed-size chunks until the server closes */
     while (1) {
         /* Attempt to read exactly chunk_size bytes or until server closes */
         int bytes_this_chunk = recv_chunk_exact(s, chunk_buf, chunk_size);
         if (bytes_this_chunk <= 0) {
             /* <= 0 => closed connection or error */
             break;
         }
 
         /* Place a '\0' terminator so functions like strstr can work */
         chunk_buf[bytes_this_chunk] = '\0';
 
         /* 5. Count occurrences of "<h1>" in this chunk only (no spanning) */
         {
             char *search_ptr = chunk_buf;
             while ((search_ptr = strstr(search_ptr, "<h1>")) != NULL) {
                 h1_count++;
                 search_ptr += 4; /* skip past "<h1>" */
             }
         }
 
         /* 6. Update total bytes received */
         total_bytes += bytes_this_chunk;
     }
 
     /* 7. Print final results */
     printf("Number of <h1> tags: %d\n", h1_count);
     printf("Number of bytes: %d\n", total_bytes);
 
     /* Clean up */
     free(chunk_buf);
     close(s);
 
     return 0;
 }
 
 /******************************************************************************
  * lookup_and_connect
  *
  * Resolves 'host' and 'service' into a socket address, attempts to connect,
  * and returns the connected socket descriptor. Returns -1 on failure.
  *
  * NOTE: This uses AF_UNSPEC so that it can connect using IPv4 or IPv6.
  ******************************************************************************/
 int lookup_and_connect(const char *host, const char *service)
 {
     struct addrinfo hints, *res, *rp;
     int sockfd;
 
     memset(&hints, 0, sizeof(hints));
     hints.ai_family   = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
     hints.ai_socktype = SOCK_STREAM;  /* Stream socket */
     hints.ai_flags    = 0;
     hints.ai_protocol = 0;            /* Any protocol */
 
     int ret = getaddrinfo(host, service, &hints, &res);
     if (ret != 0) {
         fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
         return -1;
     }
 
     /* Try each address until we successfully connect */
     for (rp = res; rp != NULL; rp = rp->ai_next) {
         sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
         if (sockfd == -1) {
             continue; /* Socket failed, try next */
         }
         if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {
             /* Success */
             freeaddrinfo(res);
             return sockfd;
         }
         close(sockfd); /* Connect failed, close this one and try next */
     }
 
     /* No address succeeded */
     freeaddrinfo(res);
     perror("connect");
     return -1;
 }
 
 /******************************************************************************
  * send_all
  *
  * Sends the entire buffer, handling possible partial sends by looping.
  * Returns 0 on success, -1 on error.
  ******************************************************************************/
 int send_all(int sockfd, const char *buf, int len)
 {
     int total_sent = 0;
     while (total_sent < len) {
         int bytes_sent = send(sockfd, buf + total_sent, len - total_sent, 0);
         if (bytes_sent < 0) {
             return -1; /* Error */
         }
         total_sent += bytes_sent;
     }
     return 0; /* Success */
 }
 
 /******************************************************************************
  * recv_chunk_exact
  *
  * Attempts to read exactly 'chunk_size' bytes from 'sockfd' into 'buf',
  * looping if necessary. However, if the server closes the connection early,
  * you may receive fewer than 'chunk_size' bytes. If you do read some data,
  * returns the number of bytes actually read. If 0 or negative, that signals
  * closed connection or error.
  *
  * This is strictly reading up to chunk_size each call to simulate partial
  * receives. If the server ends the connection, we stop reading.
  ******************************************************************************/
 int recv_chunk_exact(int sockfd, char *buf, int chunk_size)
 {
     int bytes_read = 0;
     while (bytes_read < chunk_size) {
         int r = recv(sockfd, buf + bytes_read, chunk_size - bytes_read, 0);
         if (r < 0) {
             /* Error */
             perror("recv");
             return -1;
         }
         if (r == 0) {
             /* Server closed connection */
             break;
         }
         bytes_read += r;
     }
     return bytes_read;
 }
 