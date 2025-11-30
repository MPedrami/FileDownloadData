#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define PORT 3456
#define MAX_BUFFER_SIZE 1024

int send_command(int sockfd, const char *command) 
{
    ssize_t bytes_sent = send(sockfd, command, strlen(command), 0);
    if (bytes_sent < 0) 
    {
        perror("Error sending command");
        return -1;
    }
    return 0;
}

char *receive_response(int sockfd) 
{
    char *response = (char *)malloc(MAX_BUFFER_SIZE);
    if (response == NULL) 
    {
        perror("ERROR allocating memory");
        return NULL;
    }
    memset(response, 0, MAX_BUFFER_SIZE);
    ssize_t bytes_received;
    char buffer[1];
    int i = 0;

    while ((bytes_received = recv(sockfd, buffer, 1, 0)) > 0) {
        if (buffer[0] == '\n') 
        {
            break;
        }
        if (i < MAX_BUFFER_SIZE - 1) 
        {
            response[i++] = buffer[0];
        } 
        else 
        {
            fprintf(stderr, "Response too long\n");
            free(response);
            return NULL;
        }
    }
    if (bytes_received < 0) 
    {
        perror("Error receiving response");
        free(response);
        return NULL;
    }
    response[i] = '\0';
    return response;
}

int main() 
{
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char server_address[100];
    char *response;
    int choice;

    printf("Enter server address (richmond.cs.sierracollege.edu or london.cs.sierracollege.edu): ");
    scanf("%s", server_address);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
        perror("Error creating socket");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    server = gethostbyname(server_address);
    if (server == NULL) 
    {
        fprintf(stderr, "Error: No such host\n");
        close(sockfd);
        exit(1);
    }

    memcpy(&serv_addr.sin_addr, server->h_addr, server->h_length);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    {
        perror("Error connecting to server");
        close(sockfd);
        exit(1);
    }

    printf("Connected to server.\n");

    // Receive the initial HELO greeting
    response = receive_response(sockfd);
    if (response) 
    {
        printf("Server says: %s\n", response);
        free(response);
    }

    while (1) 
    {
        printf("\nMenu:\n");
        printf("1. List Files\n");
        printf("2. Download File\n");
        printf("3. Quit\n");
        printf("Make your choice: ");
        scanf("%d", &choice);

        switch (choice) 
        {
            case 1:
                if (send_command(sockfd, "LIST\n") < 0) 
                {
                    close(sockfd);
                    exit(1);
                }
                response = receive_response(sockfd);
                if (response == NULL) 
                {
                    close(sockfd);
                    exit(1);
                }
                if (strncmp(response, "+OK", 3) == 0) 
                {
                    printf("Files:\n");
                    char *line = NULL;
                    size_t len = 0;
                    ssize_t read;

                    // Read file listing line by line
                    while (1) 
                    {
                        line = receive_response(sockfd);
                        if (line == NULL) 
                        {
                            break;
                        }
                        if (strcmp(line, ".") == 0) 
                        {
                            free(line);
                            break; // End of listing
                        }
                        printf("%s\n", line);
                        free(line);
                    }
                } 
                else 
                {
                    printf("Error: %s\n", response);
                }
                free(response);
                break;

            case 2:
                printf("Enter the filename to download: ");
                char filename[100];
                scanf("%s", filename);
                if (access(filename, F_OK) == 0)
                {
                    char overwrite_choice;
                    printf("File already exits. Overwrite? (y/n): ");
                    scanf(" %c", &overwrite_choice);
                    if (overwrite_choice != 'y')
                    {
                        printf("Download cnacelled.\n");
                        break;
                    }
                }
                char command[120];
                snprintf(command, sizeof(command), "GET %s\n", filename);
                if (send_command(sockfd, command) < 0)
                {
                    perror("failed to send command");
                    break;
                }
                response = receive_response(sockfd);
                if (response == NULL)
                {
                    perror("Failed to receive response");
                    break;
                }
                if (strncmp(response, "+OK", 3) == 0)
                {
                    int file_size;
                    sscanf(response + 4, "%d", &file_size);
                    printf("Downloading %s (%d bytes)...\n", filename, file_size);

                    FILE *file = fopen(filename, "wb");
                    if (file == NULL)
                    {
                        perror("Error opening file for writing");
                        free(response);
                        break;
                    }
                    char buffer[MAX_BUFFER_SIZE];
                    int total_received = 0;
                    int percentage = 0;
                    char progress_bar[51];
                    memset(progress_bar, 0, sizeof(progress_bar));
                    while (total_received < file_size)
                    {
                        ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
                        if(bytes_received < 0)
                        {
                            perror("Error receiving file data");
                            fclose(file);
                            free(response);
                            break;
                        }
                        fwrite(buffer, 1, bytes_received, file);
                        total_received += bytes_received;
                        percentage = (total_received * 100) / file_size;
                        int bar_length = (int)((float)percentage / 2.0);
                        memset(progress_bar, '#', bar_length);
                        progress_bar[bar_length] = '\0';
                        printf("\rDownloading %s: [%-50s] %d%%", filename, progress_bar, percentage);
                        fflush(stdout);
                    }
                    fclose(file);
                    printf("\rDownloading %s: [%-50s] 100%%\n", filename, progress_bar);
                    printf("File downloaded successfully: %s\n", filename);
                }
                else 
                {
                    printf("Error: %s\n", response);
                }
                free(response);
                break;

            case 3:
                if (send_command(sockfd, "QUIT\n") < 0) 
                {
                    close(sockfd);
                    exit(1);
                }
                close(sockfd);
                printf("Goodbye!\n");
                exit(0);

            default:
                printf("Invalid choice. Please try again.\n");
        }
    }
    close(sockfd);
    return 0;
}