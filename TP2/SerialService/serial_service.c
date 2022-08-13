/**
 * @file serial_service.c
 * @author Gianfranco Talocchino (gftalocchino@gmail.com)
 * @brief SOPG TP2
 * @version 0.2
 * @date 2022-08-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "SerialManager.h"

#define SERIAL_READ_POLLING_PERIOD_US (500000u)
#define TCP_PORT                      (10000u)
#define IP_ADDRESS                    "127.0.0.1"


int listen_fd = -1;
int conn_fd = -1;

bool is_tcp_conn_open = false;
bool is_serial_conn_open = false;
bool is_running = true;

pthread_t serial_to_tcp_thread;
pthread_mutex_t mutex_serial_conn_open = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_tcp_conn_open = PTHREAD_MUTEX_INITIALIZER;


bool read_tcp_conn_open_flag(void);
void set_tcp_conn_open_flag(bool);
bool read_serial_conn_open(void);
void set_serial_conn_open(bool);
void signal_handler(int);
void init_signals(void);
void mask_signals(int);
void start_tcp_server(void);
void start_serial(void);
void launch_threads(void);
void wait_threads_finish(void);
void tcp_to_serial(void);
void *serial_to_tcp(void *);


int main(void) {
   /* Setting up singnal handlers for SIGINT and SIGTERM. */
   init_signals();

   puts("Starting serial service");

   /* Opening serial port. */
   start_serial();

   /* Starting TCP server. */
   start_tcp_server();

   /* Masking SIGINT and SIGTERM prior to launch threads. */
   mask_signals(SIG_BLOCK);

   /* Launching threads. */
   launch_threads();

   /* Unblock signals for this thread. */
   mask_signals(SIG_UNBLOCK);

   /* Calling TCP to serial loop. */
   tcp_to_serial();

   /* Waiting for the threadd to terminate. */
   wait_threads_finish();

   /*Closing serial port. */
   serial_close();

   puts("Threads terminated successfully");

   return EXIT_SUCCESS;
}

bool read_tcp_conn_open_flag(void) {
   pthread_mutex_lock(&mutex_tcp_conn_open);
   bool retval = is_tcp_conn_open ? true : false;
   pthread_mutex_unlock(&mutex_tcp_conn_open);

   return retval;
}

void set_tcp_conn_open_flag(bool value) {
   pthread_mutex_lock(&mutex_tcp_conn_open);
   is_tcp_conn_open = value;
   pthread_mutex_unlock(&mutex_tcp_conn_open);
}

bool read_serial_conn_open(void) {
   pthread_mutex_lock(&mutex_serial_conn_open);
   bool retval = is_serial_conn_open ? true : false;
   pthread_mutex_unlock(&mutex_serial_conn_open);

   return retval;
}

void set_serial_conn_open(bool value) {
   pthread_mutex_lock(&mutex_serial_conn_open);
   is_serial_conn_open = value;
   pthread_mutex_unlock(&mutex_serial_conn_open);
}

void signal_handler(int sig) {
   char *signal_msg = (sig == SIGINT) ? "Received SIGINT\n" 
                                      : "Received SIGTERM\n";                          
   write(1, signal_msg, strlen(signal_msg));

   char *finishing_msg = "Closing serial service...\n";
   write(1, finishing_msg, strlen(finishing_msg));
  
   is_running = false;
}

void init_signals(void) {
   /* Setting up the same singnal handler for SIGINT and SIGTERM. */
   struct sigaction action = {.sa_handler = signal_handler};
   
   sigemptyset(&action.sa_mask);
   
   if (sigaction(SIGINT, &action, NULL) < 0) {
      perror("SIGINT sigaction");
      exit(EXIT_FAILURE);
   }

   if (sigaction(SIGTERM, &action, NULL) < 0) {
      perror("SIGTERM sigaction");
      exit(EXIT_FAILURE);
   }
}

void mask_signals(int how) {
   sigset_t set;

   sigemptyset(&set);
   sigaddset(&set, SIGTERM);
   sigaddset(&set, SIGINT);
    
   int error = pthread_sigmask(how, &set, NULL);

   if (error != 0) {
      printf("pthread_sigmask: %s\n", strerror(error));
      exit(EXIT_FAILURE);
   }
}

void start_serial(void) {
   puts("Opening serial port");

   if(serial_open(1, 115200) != 0) {
      puts("serial_open: error");
      exit(EXIT_FAILURE);
   }

   set_serial_conn_open(true);
}

void start_tcp_server(void) {
   puts("Initializing TPC server");

   listen_fd = socket(AF_INET, SOCK_STREAM, 0);

   if (listen_fd < 0) {
      perror("socket");
      exit(EXIT_FAILURE);
   }

   struct sockaddr_in server = {
      .sin_family = AF_INET,
      .sin_port = htons(TCP_PORT)
   };

   if (inet_pton(AF_INET, IP_ADDRESS, &server.sin_addr) < 0) {
      perror("inet_pton");
      exit(EXIT_FAILURE);
   }

   if (bind(listen_fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
      perror("bind");
      exit(EXIT_FAILURE);
   }

   if (listen(listen_fd, 10) < 0) {
      perror("inet_pton");
      exit(EXIT_FAILURE);
   }
}

void launch_threads(void) {
   puts("Launching threads");

   int error = pthread_create(&serial_to_tcp_thread, NULL, serial_to_tcp, NULL);

   if (error != 0) {
      printf("pthread_create: serial_to_tcp: %s\n", strerror(error));
      exit(EXIT_FAILURE);
   }
} 

void wait_threads_finish(void) {
   int error = pthread_cancel(serial_to_tcp_thread);

  if (error != 0) {
      printf("pthread_cancel: %s\n", strerror(error));
      exit(EXIT_FAILURE);
   }

	error = pthread_join(serial_to_tcp_thread, NULL);

   if (error != 0) {
      printf("pthread_join: %s\n", strerror(error));
      exit(EXIT_FAILURE);
   }
}

void tcp_to_serial(void) {
   puts("Running TCP to serial loop");

   while (is_running) {
      struct sockaddr_in client = {0};
      socklen_t client_len = sizeof(client);
      
      puts("tcp_to_serial: waiting connection");
      conn_fd = accept(listen_fd, (struct sockaddr *) &client, &client_len);

      if (conn_fd < 0) {
         set_tcp_conn_open_flag(false);
         perror("tcp_to_serial: accept");
      } else {
         set_tcp_conn_open_flag(true);
         char ip_client[24];
         inet_ntop(AF_INET, &client.sin_addr, ip_client, sizeof(ip_client));
         printf("tcp_to_serial: connection from: %s:%u\n", ip_client, ntohs(client.sin_port));
      }

      while (read_tcp_conn_open_flag()) {
         char read_buffer[32];
         ssize_t read_bytes = read(conn_fd, read_buffer, sizeof(read_buffer));

         if (read_bytes < 0) {
            perror("tcp_to_serial: read");
            is_tcp_conn_open = false;
         }

         if (read_bytes == 0) {
            puts("tcp_to_serial: connection closed");
            is_tcp_conn_open = false;
         }

         if (read_bytes > 0) {
            read_buffer[read_bytes] = 0;
            printf("tcp_to_serial: TCP -> serial: %s", read_buffer);
                  
            if (read_serial_conn_open()) {
               serial_send(read_buffer, read_bytes);
            } else {
               puts("tcp_to_serial: serial connection is not open");
            }
         }
      }

      close(conn_fd);
   }

   puts("Finishing TCP to serial loop");
}

void *serial_to_tcp(void *arg) {
   puts("Starting serial to tcp thread");

   while (1) {
      char read_buffer[32];
      int read_bytes = serial_receive(read_buffer, sizeof(read_buffer));

      if (read_bytes == 0) {
         puts("serial_to_tcp: serial connection closed");
         set_serial_conn_open(false);
      }

      if (read_bytes > 0) {
         read_buffer[read_bytes] = 0;
         printf("serial_to_tcp: serial -> TCP: %s", read_buffer);

         if (read_tcp_conn_open_flag()) {
            write(conn_fd, read_buffer, strlen(read_buffer));
         } else {
            puts("serial_to_tcp: TCP connection is not open");
         }
      }
      
      usleep(SERIAL_READ_POLLING_PERIOD_US);
   }

   return NULL;
}
