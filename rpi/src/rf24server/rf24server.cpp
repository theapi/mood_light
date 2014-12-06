/**
 *
 * A socket server that listens for mumerical commands
 * and forwards them to listening nRF24L01+ nodes.
 *
 * Normally this is stateless so one number sent per connection.
 *
 * To keep the connection open the first command must be 2
 *   to which the response will be 201
 * To close a connection opened with the "2" command, send 3
 *
 * Any number greater than 31 is passed to the listening nodes.
 *
 * If it has not been possible to write to the radio receiver
 * the return code will be 504.
 *
*/
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <arpa/inet.h>
#include <strings.h>

/**
 * Adapted from
 * https://github.com/TMRh20/RF24/blob/master/examples_RPi/gettingstarted.cpp
 */
// RF24
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <RF24/RF24.h>

// If a failure has been detected, it usually indicates a hardware issue.
// @see RF24.h
#define FAILURE_HANDLING

// Maximum size of incomming message
#define MAXMSG  32

using namespace std;
//
// Hardware configuration
//

// CE Pin, CSN Pin, SPI Speed

// Setup for GPIO 22 CE and CE1 CSN with SPI Speed @ 1Mhz
//RF24 radio(RPI_V2_GPIO_P1_22, RPI_V2_GPIO_P1_26, BCM2835_SPI_SPEED_1MHZ);

// Setup for GPIO 22 CE and CE0 CSN with SPI Speed @ 4Mhz
//RF24 radio(RPI_V2_GPIO_P1_15, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_4MHZ);

// NEW: Setup for RPi B+
//RF24 radio(RPI_BPLUS_GPIO_J8_15,RPI_BPLUS_GPIO_J8_24, BCM2835_SPI_SPEED_8MHZ);

// Setup for GPIO 22 CE and CE0 CSN with SPI Speed @ 8Mhz
RF24 radio(RPI_V2_GPIO_P1_15, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_8MHZ);


// Addresses of potential listening nodes
const uint8_t pipes[][6] = {"AAAAA", "BBBBB", "CCCCC", "DDDDD", "EEEEE", "FFFFF", "HHHHH"};
// Default to 4 listening nodes.
uint8_t num_clients = 4;
uint8_t num_clients_max = 7;

void dostuff(int); /* function prototype */
int communicate(int, bool&);
bool respond(int, const char*);

void error(const char *msg)
{
  perror(msg);
  exit(1);
}

int make_socket (uint16_t port)
{
  int sock;
  struct sockaddr_in name;

  /* Create the socket. */
  sock = socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    {
      perror ("socket");
      exit (EXIT_FAILURE);
    }

  /* Give the socket a name. */
  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);
  if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
      perror ("bind");
      exit (EXIT_FAILURE);
    }

  return sock;
}

int read_from_client (int sock, RF24 radio)
{
  char buffer[MAXMSG];
  int nbytes;

  bzero(buffer, MAXMSG);
  nbytes = read (sock, buffer, MAXMSG);
  if (nbytes < 0)
    {
      /* Read error. */
      perror ("read");
      exit (EXIT_FAILURE);
    }
  else if (nbytes == 0)
    /* End-of-file. */
    return -1;
  else
    {
      /* Data read. */
      //fprintf (stderr, "Server: got message: `%s'\n", buffer);


      // Take the input as an int,
      // chop it to a short (2 bytes).
      // So maximum code number is 32767 (int on 16bit arduino)
      short code = atoi(buffer);

      // Response codes are http status codes.
      if (code > 0) {

        // Handle the code
        if (code > 31) {
          printf("%hd:", code);
          // Send it to the clients via the nRF24L01+.
          for (int i = 0; i < num_clients; i++) {
            // Send message to the node on the pipe address
            radio.stopListening();
            radio.openWritingPipe(pipes[i]);

            //printf("\n%s -> %hd", pipes[i], code);
            printf(" %s ", pipes[i]);
            bool ok = radio.write( &code, 2 );
            if (!ok) {
              respond(sock, "504");
              printf("(0)");
            } else {
              respond(sock, "200");
              printf("(1)");
            }
          }
          printf("\n");

        } else if (code == 3) {
          // Close the connection
          return -1;
        } else if (code == 2) {
          // Keep the connection open
          if (!respond(sock, "201")) {
            return -1;
          }
        }

      } else {
        // Bad request
        if (!respond(sock, "400")) {
          return -1;
        }
      }

      return 0;
    }
}


int main(int argc, char *argv[])
{

  //signal(SIGCHLD, SIG_IGN); // No zombies

  uint16_t port;
  /*
  int sockfd, newsockfd, portno, pid;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;
*/

  if (argc < 2) {
     fprintf(stderr,"ERROR, no port provided\n");
     exit(1);
  }
  port = atoi(argv[1]);

  // Set the number of clients if given.
  // eg; sudo ./rf24server.cpp 2000 2 (for 2 clients on port 2000)
  if (argc > 2) {
    num_clients = atoi(argv[2]);
    if (num_clients > num_clients_max) {
      num_clients = num_clients_max;
    }
  }

  extern int make_socket (uint16_t port);
  int sock;
  fd_set active_fd_set, read_fd_set;
  int i;
  struct sockaddr_in clientname;
  size_t size;

  /* Create the socket and set it up to accept connections. */
  sock = make_socket (port);
  if (listen (sock, 1) < 0)
    {
      perror ("listen");
      exit (EXIT_FAILURE);
    }

  /* Initialize the set of active sockets. */
  FD_ZERO (&active_fd_set);
  FD_SET (sock, &active_fd_set);

  printf("\nListening for commands on %d...\n", port);


/***** RF24 *********/

  // Setup and configure rf radio
  radio.begin();
  // 2 byte payload
  radio.setPayloadSize(2);
  // Ensure autoACK is enabled
  radio.setAutoAck(1);
  // Try a few times to get the message through
  radio.setRetries(0,15);
  // Dump the configuration of the rf unit for debugging
  radio.printDetails();


/**** end rf24 ***/



  while (1)
    {
      /* Block until input arrives on one or more active sockets. */
      read_fd_set = active_fd_set;
      if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
        {
          perror ("select");
          exit (EXIT_FAILURE);
        }

      /* Service all the sockets with input pending. */
      for (i = 0; i < FD_SETSIZE; ++i)
        if (FD_ISSET (i, &read_fd_set))
          {
            if (i == sock)
              {
                /* Connection request on original socket. */
                int new_client;
                size = sizeof (clientname);
                new_client = accept (sock,
                              (struct sockaddr *) &clientname,
                              &size);
                if (new_client < 0)
                  {
                    perror ("accept");
                    exit (EXIT_FAILURE);
                  }
                printf("Server: connect from host %s, port %hd.\n",
                      inet_ntoa (clientname.sin_addr),
                      ntohs (clientname.sin_port));
                FD_SET (new_client, &active_fd_set);
              }
            else
              {
                /* Data arriving on an already-connected socket. */
                if (read_from_client (i, radio) < 0)
                  {
                    printf("\nClose socket: %d...\n", i);
                    close (i);
                    FD_CLR (i, &active_fd_set);
                  }
              }
          }
    }

}

/**
 * Send the 3 character response and check for failure.
 */
bool respond(int sock, const char *msg)
{
  int n = write(sock, msg, 3);
  if (n < 0) {
    error("ERROR writing to socket");
    return 0;
  }
  return 1;
}
