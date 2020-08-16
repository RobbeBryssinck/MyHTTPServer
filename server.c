#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

#define PORT 80
#define MAX_REQUEST_SIZE 5000
#define MAX_RESOURCE_SIZE 5000
#define WEBROOT "./webroot"

void process_request(int, struct sockaddr_in *);
int get_request(int, unsigned char *);
int send_string(int, unsigned char *);
int get_file_size(int);

int main(int argc, char const *argv[])
{
    int listen_sock, accept_sock, yes=1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(server_addr);

    if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Error while creating socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        perror("Error while setting socket options");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    memset(server_addr.sin_zero, '\0', 8);

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("Error while binding socket");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_sock, 10) == -1)
    {
        perror("Error while setting up listen");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        accept_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addrlen);
        if (accept_sock == -1) {
            perror("Error while accepting connection");
            exit(EXIT_FAILURE);
        }

        process_request(accept_sock, &client_addr);
    }
}

void process_request(int accept_sock, struct sockaddr_in *client_addr_ptr) {
    unsigned char *ptr, request[MAX_REQUEST_SIZE], resource[MAX_RESOURCE_SIZE];
    int response_file, length;

    length = get_request(accept_sock, request);
    printf("Got request from %s:%d \"%s\"\n", inet_ntoa(client_addr_ptr->sin_addr), ntohs(client_addr_ptr->sin_port), request);

    if (length == -1) {
        printf("The request was too long! Terminating connection...\n");
        shutdown(accept_sock, SHUT_RDWR);
        return;
    }

    printf("%s\n", request);
    ptr = strstr(request, " HTTP/");
    if (ptr == NULL) {
        printf(" NOT HTTP!");
    } else {
        *ptr = 0;
        ptr = NULL;
        if (strncmp(request, "GET ", 4) == 0)
            ptr = request+4;
        if (strncmp(request, "HEAD ", 5) == 0)
            ptr = request+5;

        if (ptr == NULL) {
            printf("\tUNKNOWN REQUEST!\n");
            // Maybe return a valid invalid request response?
        } else {
            if (ptr[strlen(ptr) - 1] == '/')
                strcat(ptr, "index.html");
            strcpy(resource, WEBROOT);
            strcat(resource, ptr);
            response_file = open(resource, O_RDONLY, 0);
            printf("\tOpening \'%s\'\t", resource);
            if (response_file == -1) {
                printf(" 404 Not found\n");
                response_file = open("NotFound404.html", O_RDONLY, 0);
            } else {
                printf(" 200 OK\n");
                send_string(accept_sock, "HTTP/1.0 200 OK\r\n");
                send_string(accept_sock, "Server: Cosideci Webserver\r\n\r\n");
                if (ptr == request + 4) {
                    if ( (length = get_file_size(response_file)) == -1) {
                        perror("Error opening response file");
                        exit(EXIT_FAILURE);
                    }
                    if ( (ptr = (unsigned char *) malloc(length)) == NULL) {
                        perror("Error allocating memory for reading resource");
                        exit(EXIT_FAILURE);
                    }
                    read(response_file, ptr, length);
                    send(accept_sock, ptr, length, 0);
                    free(ptr);
                }
                close(response_file);
            }
        }
    }
    shutdown(accept_sock, SHUT_RDWR);
}

int get_request(int accept_sock, unsigned char *request) {
#define EOL "\r\n\r\n"
#define EOL_SIZE 4
    unsigned char *ptr;
    int i = 0;
    int eol_matched = 0;

    ptr = request;
    while (recv(accept_sock, ptr, 1, 0) == 1) {
        if (i >= MAX_REQUEST_SIZE)
            return -1;
        if (*ptr == EOL[eol_matched]) {
            eol_matched++;
            if (eol_matched == EOL_SIZE) {
                *(ptr+1-EOL_SIZE) = '\0';
                return strlen(request);
            }
        } else {
            eol_matched = 0;
        }
        ptr++;
        i++;
    }
}

int send_string(int accept_sock, unsigned char *buffer) {
    int sent_bytes, bytes_to_send;
    bytes_to_send = strlen(buffer);
    while (bytes_to_send > 0) {
        sent_bytes = send(accept_sock, buffer, bytes_to_send, 0);
        if (sent_bytes == -1)
            return 0; // Return 0 on send error.
        bytes_to_send -= sent_bytes;
        buffer += sent_bytes;
    }
    return 1;
}

int get_file_size(int fd) {
    struct stat stat_struct;

    if (fstat(fd, &stat_struct) == -1)
        return -1;
    return (int) stat_struct.st_size;
}
