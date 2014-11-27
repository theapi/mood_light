/* A simple server in the internet domain using TCP
   The port number is passed as an argument
   This version runs forever, forking off a separate
   process for each connection

http://www.linuxhowtos.org/C_C++/socket.htm

*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#include <arduPi.h>

//  g++ -lrt -lpthread  -I/home/pi/arduPi /home/pi/arduPi/arduPi.cpp src/server/server.cpp -o server

void dostuff(int); /* function prototype */
int communicate(int);
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
     while (1) {
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

   while (communicate(sock)) {
      // todo: implement socket still connect check (write() occasionally)

      // todo: close open sockets on ctl C (if possible) to stop:
      // "ERROR on binding: Address already in use"
   }
}

int communicate (int sock)
{
   int n;
   char buffer[256];

   bzero(buffer,256);
   n = read(sock,buffer,255);
   if (n < 0) {
       error("ERROR reading from socket");
       return 0;
   }

   // Send bye to close the connection
   char *pos = strstr(buffer, "bye");
   if (pos - buffer == 0) {
	   printf("Sent: bye\n");
	   return 0;
   }
   
   printf("Got: %s",buffer);
   n = write(sock,"OK",2);
   if (n < 0) {
       error("ERROR writing to socket");
       return 0;
   }

   return 1;
}
