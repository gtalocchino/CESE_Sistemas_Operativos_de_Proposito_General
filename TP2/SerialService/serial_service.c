/**
 * @file serial_service.c
 * @author Gianfranco Talocchino (gftalocchino@gmail.com)
 * @brief 
 * @version 0.1
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
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "SerialManager.h"

#define SLEEP_TIME_SERIAL_MS (1000u)

int listen_fd = -1;
int conn_fd = -1;
bool is_tcp_conn_open = false;
bool is_serial_conn_open = false;


void start_tcp_server(void);
void launch_threads(void);
void *tcp_to_serial(void *);
void *serial_to_tcp(void *);


int main(void) {
   puts("Starting serial service");


   start_tcp_server();

   launch_threads();   

   return EXIT_SUCCESS;
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
      .sin_port = htons(10000)
   };

   if (inet_pton(AF_INET, "127.0.0.1", &server.sin_addr) < 0) {
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

   pthread_t tcp_to_serial_thread;
   pthread_t serial_to_tcp_thread;

   pthread_create(&tcp_to_serial_thread, NULL, tcp_to_serial, NULL);
   pthread_create(&serial_to_tcp_thread, NULL, serial_to_tcp, NULL);

   pthread_join(tcp_to_serial_thread, NULL);
	pthread_join(serial_to_tcp_thread, NULL);
} 

void *tcp_to_serial(void *arg) {
   puts("Starting tcp to serial thread");
   
   while (1) {
      struct sockaddr_in client = {0};
      socklen_t client_len = sizeof(client);
      
      puts("tcp_to_serial: waiting connection");
      conn_fd = accept(listen_fd, (struct sockaddr *) &client, &client_len);

      char ip_client[32];
      inet_ntop(AF_INET, &client.sin_addr, ip_client, sizeof(ip_client));
      printf("tcp_to_serial: connection from: %s:%u\n", ip_client, ntohs(client.sin_port));

      is_tcp_conn_open = true;

      while (1) {
         char read_buffer[32];
         ssize_t read_bytes = read(conn_fd, read_buffer, sizeof(read_buffer));

         if (read_bytes < 0) {
            perror("tcp_to_serial: read");
            close(conn_fd);
            is_tcp_conn_open = false;
            break;
         }

         if (read_bytes == 0) {
            puts("tcp_to_serial: connection closed");
            close(conn_fd);
            is_tcp_conn_open = false;
            break;
         }

         read_buffer[read_bytes] = 0;
         printf("tcp_to_serial: TCP -> serial: %s", read_buffer);
               
         if (is_serial_conn_open) {
            serial_send(read_buffer, read_bytes);
         } else {
            puts("tcp_to_serial: serial connection is not open");
         }
      } 
   }

   return NULL;
}

void *serial_to_tcp(void *arg) {
   puts("Starting serial to tcp thread");
   

   while (1) {
      if(serial_open(1, 115200) != 0) {
         puts("serial_to_tcp: serial_open error");
         usleep(SLEEP_TIME_SERIAL_MS);
         continue;
      }

      is_serial_conn_open = true;

      while (1) {
         char read_buffer[32];
         int read_bytes = serial_receive(read_buffer, sizeof(read_buffer));

         if (read_bytes == 0) {
            puts("serial_to_tcp: serial connection closed");
            serial_close();
            is_serial_conn_open = false;
            break;
         }

         if (read_bytes < 0) {
            usleep(SLEEP_TIME_SERIAL_MS);
            continue;
         }

         read_buffer[read_bytes] = 0;
         printf("serial_to_tcp: serial -> TCP: %s", read_buffer);
         
         if (is_tcp_conn_open) {
            write(conn_fd, read_buffer, strlen(read_buffer));
         } else {
            puts("serial_to_tcp: TCP connection is not open");
         }

         usleep(SLEEP_TIME_SERIAL_MS);
      }
   }

   return NULL;
}