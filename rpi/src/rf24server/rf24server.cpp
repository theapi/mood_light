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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

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


// Radio pipe addresses for the 2 nodes to communicate.
const uint8_t pipes[][6] = {"1Node","2Node"};


void dostuff(int); /* function prototype */
int communicate(int, bool&);
bool respond(int, const char*);

void error(const char *msg)
{
  perror(msg);
  exit(1);
}

int main(int argc, char *argv[])
{

  signal(SIGCHLD, SIG_IGN); // No zombies

  int sockfd, newsockfd, portno, pid;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;

  if (argc < 2) {
     fprintf(stderr,"ERROR, no port provided\n");
     exit(1);
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");
  bzero((char *) &serv_addr, sizeof(serv_addr));
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  if (bind(sockfd, (struct sockaddr *) &serv_addr,
          sizeof(serv_addr)) < 0)
          error("ERROR on binding");
  listen(sockfd,5);
  clilen = sizeof(cli_addr);

  printf("\nListening for commands on %d...\n", portno);


/***** RF24 *********/

  // Setup and configure rf radio
  radio.begin();
  // 2 byte payload
  radio.setPayloadSize(2);
  // Ensure autoACK is enabled
  radio.setAutoAck(1);
  // optionally, increase the delay between retries & # of retries
  radio.setRetries(15,15);
  // Dump the configuration of the rf unit for debugging
  radio.printDetails();


  /***********************************/
  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.
  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1,pipes[1]);


/**** end rf24 ***/

  while (1) {
    // Socket server
    newsockfd = accept(sockfd,
      (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0)
      error("ERROR on accept");
    pid = fork();
    if (pid < 0)
      error("ERROR on fork");
    if (pid == 0)  {
      close(sockfd);
      dostuff(newsockfd);
      exit(0);
    }
    else close(newsockfd);

  } /* end of while */
  close(sockfd);
  return 0; /* we never get here */
}

/******** DOSTUFF() *********************
 There is a separate instance of this function
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void dostuff (int sock)
{
  bool keep_alive = false;

  while (communicate(sock, keep_alive)) {
    // todo: implement socket still connect check (write() occasionally)

    // todo: close open sockets on ctl C (if possible) to stop:
    // "ERROR on binding: Address already in use"
  }

  close(sock);
}

/**
 * Protocol:
 *  Only acts on 16bit numbers (short)
 *    2 = Start session (ascii "start of text"):
 *        The server will not automatically disconnect after reading the number.
 *    3 = End session (ascii "end of text"):
 *        The server will disconnect
 *
 *    >= 32 Gets forwarded via the radio
 *
 *
 */
int communicate (int sock, bool &keep_alive)
{

  int n;
  char buffer[32];


  bzero(buffer, 32);
  n = read(sock, buffer, 31);
  if (n < 0) {
    error("ERROR reading from socket");
    return 0;
  }

  printf("Got: %s", buffer);

  // Take the input as an int,
  // chop it to a short (2 bytes).
  // So maximum code number is 32767 (int on 16bit arduino)
  short code = atoi(buffer);

  // Response codes are http status codes.
  if (code > 0) {

    // Handle the code
    if (code > 31) {

      // Send it to the arduino via the nRF24L01+.
      printf("RF24 -> %hd\n", code);
      bool ok = radio.write( &code, 2 );
      if (!ok) {
        printf("  failed.\n");
        respond(sock, "504");
      } else {
        respond(sock, "200");
      }

      if (!keep_alive) {
        // Stateless request so close the connection now.
        return 0;
      }

    } else if (code == 3) {
      // Close the connection
      return 0;
    } else if (code == 2) {
      // Keep the connection open
      keep_alive = true;
      if (!respond(sock, "201")) {
        return 0;
      }
    }

  } else {
    // Bad request
    if (!respond(sock, "400")) {
      return 0;
    }
  }

  return 1;
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
