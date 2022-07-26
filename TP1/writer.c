/**
 * @file writer.c
 *
 * @brief Writer for SOPG Practice 1
 *
 * @author Gianfranco Talocchino
 * Contact: gftalocchino@gmail.com
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define FIFO_NAME     "fifo_tp1"
#define MSG_SIGUSR1   "SIGN:1"
#define MSG_SIGUSR2   "SIGN:2"
#define DATA_HEADER   "DATA:"
#define HEADER_LENGTH (5)
#define INPUT_SIZE    (256)


int fifo_fd;


void sigusr_handler(int sig) {
   const char *msg = (sig == SIGUSR1) ? MSG_SIGUSR1 : MSG_SIGUSR2;
   write(fifo_fd, msg, strlen(msg));
}

int main(void) {
   puts("Starting writer");

   /* Creating and opening a named FIFO. */
   if (mknod(FIFO_NAME, S_IFIFO | 0666, 0) < 0) {
      perror("mknod");
      if (errno != EEXIST) {
         exit(EXIT_FAILURE);
      }
   }

   puts("Waiting for readers...");
   
   fifo_fd = open(FIFO_NAME, O_WRONLY);

   if (fifo_fd < 0) {
      perror("open");
      exit(EXIT_FAILURE);
   }

   puts("Got a reader");

   /* Setting up singnal handlers. */
   struct sigaction sa_sigusr = {
      .sa_handler = sigusr_handler,
      .sa_flags = SA_RESTART
   };

   sigemptyset(&sa_sigusr.sa_mask);
   
   if (sigaction(SIGUSR1, &sa_sigusr, NULL) < 0) {
      perror("SIGUSR1 sigaction");
      exit(EXIT_FAILURE);
   }

   if (sigaction(SIGUSR2, &sa_sigusr, NULL) < 0) {
      perror("SIGUSR2 sigaction");
      exit(EXIT_FAILURE);
   }

   char input_buf[INPUT_SIZE];
   strcpy(input_buf, DATA_HEADER);
   char *input = input_buf + HEADER_LENGTH;

   while (1) {
      puts("Type a message:");
      printf("> ");

      if (fgets(input, sizeof(input_buf) - HEADER_LENGTH, stdin) == NULL) {
         perror("fgets");
         exit(EXIT_FAILURE);
      }
      
      if (write(fifo_fd, input_buf, strlen(input_buf)) < 0) {
         perror("write");
         exit(EXIT_FAILURE);
      }
   } 

   return EXIT_SUCCESS;
}
