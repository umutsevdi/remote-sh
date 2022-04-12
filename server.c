#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "misc.h"

#define LINE_SIZE 255
#define SCREEN_SIZE 1024
#define PRM_LEN 32
#define PRM_NUM 100
#define BACK_LOG 5

int main(int argc, char *argv[]) {

  int i, n, child_pid;
  int opt, argCnt = 0, processCnt;
  int sockfd, newsockfd, portno, clilen;  // int variable that is used later
  struct sockaddr_in serv_addr, cli_addr; // calling the library struct
  char buffer[LINE_SIZE];                 // buffer of size created
  char **args;
  char prog[32];
  while ((opt = getopt(argc, argv, "n:p:")) != -1) {
    switch (opt) {
    case 'n':
      processCnt = atoi(optarg);
      argCnt++;
      break;
    case 'p':
      portno = atoi(optarg);
      argCnt++;
      break;
    default: /* '?' */
      fprintf(stderr, "Usage: %s [-a ip_address] [-p port_number] \n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (argCnt != 2) {
    error("ERR>> missing arguments");
  }

  // 1st IP Address 2nd TCP Concept 3rd Socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    error("ERR>> socket creation failed");
  }

  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  // convert and use port number
  serv_addr.sin_port = htons(portno);

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    error("ERR>> can not bind to socket");
  }

  listen(sockfd, BACK_LOG);
  clilen = sizeof(cli_addr);

  //
  // The new socket for the client informations
  //
  while ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr,
                             (socklen_t *)&clilen))) {
    if (newsockfd < 0) {
      error("ERR>> can not accept");
    }
    if (!fork()) {
      while (1) {
        bzero(buffer, LINE_SIZE); // Clears the buffer
        n = recv(newsockfd, buffer, LINE_SIZE, 0);
        if (n < 0) {
          error("ERR>> can not read from socket");
        }

        // Buffer Stores the msg sent by the client
        printf("Here is the entered bash command: %s\n", buffer);

        // n = send(newsockfd, "I got your message", 18, 0);
        if (n < 0) {
          error("ERROR writing to socket");
        }

        args = malloc(PRM_NUM * sizeof(char *));
        for (i = 0; i < PRM_NUM; i++)
          args[i] = malloc(PRM_LEN * sizeof(char));

        // Running the Bash Commands
        if (readAndParseCmdLine(buffer, prog, args)) {
          int pipefd[2]; // creating pipe to communicate between parent and
                         // child processes
          char pipebuffer[SCREEN_SIZE];
          pipe(pipefd);

          child_pid = fork();

          if (child_pid == 0) { // child part
            // Set pipe to write end and close read end
            close(pipefd[0]);
            dup2(pipefd[1], 1);
            dup2(pipefd[1], 2);

            execvp(prog, args); // create and run the new process and close the
                                // child process
            printf("Error in excuting the command- please make sure you type "
                   "the right syntax.\n");
          } else { // parent part
            wait(&child_pid);
            close(pipefd[1]);
            // read returns buffer_size
            int buffer_size = read(pipefd[0], pipebuffer, sizeof(pipebuffer));
            pipebuffer[buffer_size] = '\0';
            printf("pipe_response: (\n%s)\n", pipebuffer);
            n = send(newsockfd, pipebuffer, buffer_size + 1, 0);
            printf("response was sent to client %d\n",newsockfd);
            if (n < 0) {
              error("ERROR writing to socket");
            }
            bzero(buffer, LINE_SIZE); // Clears the buffer
          }
        }
      }
    }
  }
}
