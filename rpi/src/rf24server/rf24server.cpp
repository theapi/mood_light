/**
 *
 * A socket server that listens for mumerical commands
 * and forwards them to listening nRF24L01+ nodes.
 *
 * If it has been possible to write to the radio receiver
 * the return code will be 200.
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
#include <sys/time.h>

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


// Size of messages sent to the radios in bytes
// According to https://www.youtube.com/watch?v=5Xhvphsj1ms&list=UUTXOorupCLqqQifs2jbz7rQ
// 8-16 byte payload is no good, 18 is good though.
#define MAX_PAYLOAD_SIZE 18

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
const uint8_t radio_clients[][6] = {"AAAAA", "BBBBB", "CCCCC", "DDDDD", "EEEEE", "FFFFF", "HHHHH"};
// Default to 4 listening nodes.
uint8_t num_clients = 4;
uint8_t num_clients_max = 7;
// Radio pipes
const uint8_t pipes[][6] = {"1BASE", "2BASE", "3BASE", "4BASE", "5BASE"};

uint16_t msg_id = 0;

/**
 * sizeof(payload_t) must be <= MAX_PAYLOAD_SIZE
 * @see RF24.h bool write()
 * The maximum size of data written is the fixed payload size, see
 * getPayloadSize().  However, you can write less, and the remainder
 * will just be filled with zeroes.
*/
typedef struct{
  uint16_t timestamp;
  uint16_t msg_id;
  uint16_t vcc;
  uint16_t a;
  uint16_t b;
  uint16_t c;
  uint16_t d;
  uint8_t type;
  uint8_t device_id;
}
payload_t;


void error(const char *msg)
{
  perror(msg);
  exit(1);
}

/**
 * Send the 3 character response and check for failure.
 */
bool respond(int sock, const char *msg)
{
  if (sock < 1) {
    // Pretent it was sent
    return 0;
  }
  int n = write(sock, msg, 3);
  if (n < 0) {
    error("ERROR writing to socket");
    return 0;
  }
  return 1;
}

int makeSocket(uint16_t port)
{
  int sock;
  struct sockaddr_in name;

  /* Create the socket. */
  sock = socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror ("socket");
    exit (EXIT_FAILURE);
  }

  /* Give the socket a name. */
  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);
  if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0) {
    perror ("bind");
    exit (EXIT_FAILURE);
  }

  return sock;
}

/**
 * Send it to the clients via the nRF24L01+.
 */
int sendPayloadToRadios(payload_t payload, int sock)
{
  printf ("%c %d:\n", payload.type, payload.timestamp);



  for (int i = 0; i < num_clients; i++) {
    // Send message to the node on the pipe address
    radio.stopListening();
    radio.openWritingPipe(radio_clients[i]);

    //printf("\n%s -> %hd", radio_clients[i], code);
    printf(" %s ", radio_clients[i]);

    // NB: seems to be that it does not send the same message twice in a row
    // BUT radio.write still returns true AND the ack payload is the same as last time.

    bool ok = radio.write( &payload, sizeof(payload));
    if (!ok) {
      respond(sock, "504");
      printf("(0)");
    } else {

      // If an ack with payload of 2 bytes was received
      while (radio.available()) {
        short ack_payload;
        radio.read( &ack_payload, sizeof(ack_payload));
        // just dump it to screen for now.
        printf("ack:%hd", ack_payload);
      }

      respond(sock, "200");
      printf("(1)");
    }

    // Continue listening
    radio.startListening();

  }
  printf("\n");

  return 0;
}

int readSocket(int sock)
{
  char buffer[MAX_PAYLOAD_SIZE];
  int nbytes;

  bzero(buffer, MAX_PAYLOAD_SIZE);
  nbytes = read (sock, buffer, MAX_PAYLOAD_SIZE);
  if (nbytes < 0) {
      // Read error.
      perror ("read");
      exit (EXIT_FAILURE);
  } else if (nbytes == 0) {
    // End-of-file.
    return -1;
  } else {
    // Send "exit" to close the connection
    char *pos = strstr(buffer, "exit");
    if (pos - buffer == 0) {
      printf("Got: exit\n");
      return -1;
    }

    struct timeval tv;
    gettimeofday(&tv,NULL);

    // Pretty much hardcoded payload for now
    payload_t payload;
    payload.device_id = 'X';
    payload.vcc = 0; // no vcc on the server
    payload.type = 'X'; // transfer from internet/socket
    payload.timestamp = tv.tv_sec;
    payload.msg_id = msg_id;
    payload.a = atoi(buffer); // Just create an int from whatever came in

    msg_id++; // Let it overflow

    // Pass the message to the radios.
    return sendPayloadToRadios(payload, sock);
  }
}


int main(int argc, char *argv[])
{
  uint16_t port;
  extern int makeSocket(uint16_t port);
  int sock;
  fd_set active_fd_set, read_fd_set;
  int i;
  struct sockaddr_in clientname;
  size_t size;
  struct timeval tv;

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

  // Create the socket and set it up to accept connections.
  sock = makeSocket(port);
  if (listen (sock, 1) < 0) {
    perror ("listen");
    exit (EXIT_FAILURE);
  }

  // Initialize the set of active sockets.
  FD_ZERO (&active_fd_set);
  FD_SET (sock, &active_fd_set);

  // Allow select to block for 200000 us
  tv.tv_sec = 0;
  tv.tv_usec = 200000;

  printf("\nListening for commands on %d...\n", port);


/***** RF24 *********/

  // Setup and configure rf radio
  radio.begin();
  // 2 byte payload
  radio.setPayloadSize(MAX_PAYLOAD_SIZE);
  // Ensure autoACK is enabled
  radio.setAutoAck(1);
  // Allow optional ack payloads
  radio.enableAckPayload();
  // Try a few times to get the message through
  radio.setRetries(0,15);

  // Listen to the pipes
  for (int i = 0; i < 5; i++) {
    radio.openReadingPipe(i+1, pipes[i]);
  }
  radio.startListening();

  // Dump the configuration of the rf unit for debugging
  radio.printDetails();

/**** end rf24 ***/


  while (1) {
    // Listen for input on one or more active sockets.
    // For a max time of tv.tv_usec
    read_fd_set = active_fd_set;
    if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, &tv) < 0) {
      perror ("select");
      exit (EXIT_FAILURE);
    }

    // Service all the sockets with input pending
    for (i = 0; i < FD_SETSIZE; ++i) {
      if (FD_ISSET (i, &read_fd_set)) {
        if (i == sock) {
          // Connection request on original socket.
          int new_client;
          size = sizeof (clientname);
          new_client = accept (sock,
                        (struct sockaddr *) &clientname,
                        &size);
          if (new_client < 0) {
            perror ("accept");
            exit (EXIT_FAILURE);
          }
          printf("Server: connect from host %s, port %hd.\n",
                inet_ntoa (clientname.sin_addr),
                ntohs (clientname.sin_port));
          FD_SET (new_client, &active_fd_set);
        } else {
          // Data arriving on an already-connected socket.
          if (readSocket(i) < 0) {
            printf("\nClose socket: %d...\n", i);
            close (i);
            FD_CLR (i, &active_fd_set);
          }
        }
      }
    }

    // Handle any messages from the radio
    while (radio.available()) {
      payload_t payload;
      radio.read(&payload, sizeof(payload));
      // Dump it to screen
      printf("payload:%c %d\n", payload.type, payload.timestamp);
      // Tell all who care
      sendPayloadToRadios(payload, 0);
    }

  }

}


