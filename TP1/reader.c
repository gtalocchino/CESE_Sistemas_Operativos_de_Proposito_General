/**
 * @file reader.c
 *
 * @brief Reader for SOPG Practice 1
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
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define FIFO_NAME            "fifo_tp1"
#define LOG_DATA_FILENAME    "log.txt"
#define LOG_SIGNALS_FILENAME "signals.txt"
#define DATE_FMT             "[%D %T]"
#define DATA_HEADER          "DATA:"
#define SIGNAL_HEADER        "SIGN:"
#define HEADER_LENGTH        (5)
#define INPUT_SIZE           (256)
#define LOG_SIZE             (512)
#define DATE_SIZE            (32)


int fifo_fd;


int main(void) {
   puts("Starting reader");

   /* Creating and opening a named FIFO. */
   if (mknod(FIFO_NAME, S_IFIFO | 0666, 0) < 0) {
      perror("mknod");
      if (errno != EEXIST) {
         exit(EXIT_FAILURE);
      }
   }

   puts("Waiting for writers...");

   fifo_fd = open(FIFO_NAME, O_RDONLY);

   if (fifo_fd < 0) {
      perror("open");
      exit(EXIT_FAILURE);
   }

   puts("Got a writer");

   /* Creating log files. */
   FILE *log_data = fopen(LOG_DATA_FILENAME, "a");

   if (log_data == NULL) {
      perror("fopen log_data");
      exit(EXIT_FAILURE);
   }

   FILE *log_signals = fopen(LOG_SIGNALS_FILENAME, "a");

   if (log_signals == NULL) {
      perror("fopen log_singals");
      exit(EXIT_FAILURE);
   }


   while (1) {
      char input_buf[INPUT_SIZE];
      ssize_t read_count = read(fifo_fd, input_buf, sizeof(input_buf));

      if (read_count == 0) {
         puts("Writer connection broken");
         exit(EXIT_FAILURE);
      }

      if (read_count == -1) {
         perror("read");
         exit(EXIT_FAILURE);
      }
      
      time_t now = time(NULL);
     
      FILE *file;
      if (memcmp(DATA_HEADER, input_buf, HEADER_LENGTH) == 0) {
         file = log_data;
         input_buf[read_count] = 0;
      } else if (memcmp(SIGNAL_HEADER, input_buf, HEADER_LENGTH) == 0) {
         file = log_signals;
         input_buf[read_count] = '\n';
         input_buf[read_count + 1] = 0;
      } else {
         file = NULL;
      }

      char timestamp_buf[DATE_SIZE];
      strftime(timestamp_buf, sizeof(timestamp_buf), DATE_FMT, localtime(&now));
      
      printf("%s %s",timestamp_buf, input_buf);

      if (file != NULL) {
         fprintf(file, "%s %s",timestamp_buf, input_buf + HEADER_LENGTH);
         fflush(file);
      }
   } 

   return EXIT_SUCCESS;
}
