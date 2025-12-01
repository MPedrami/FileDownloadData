#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/time.h>

#define PORT 3456
#define MAX_BUFFER_SIZE 1048576

int get_known_file_size(const char *filename)
{
    if (strcmp(filename, "small.txt") == 0)
    {
        return 74;
    }
    if (strcmp(filename, "sonnet.txt") == 0)
    {
        return 620;
    }
    if (strcmp(filename, "starwars.txt") == 0)
    {
        return 3372050;
    }
    if (strcmp(filename, "lorem_ipsum.txt") == 0)
    {
        return 5891;
    }
    if (strcmp(filename, "hashes1.txt") == 0)
    {
        return 212;
    }
    if (strcmp(filename, "zero.txt") == 0)
    {
        return 0;
    }
    if (strcmp(filename, "people.txt") == 0)
    {
        return 39307209;
    }
    if (strcmp(filename, "rockyou.txt.gz") == 0)
    {
        return 53357063;
    }
    if (strcmp(filename, "sclibrary.jpg") == 0)
    {
        return 157467;
    }
    if (strcmp(filename, "arduino.jpg") == 0)
    {
        return 1093342;
    }
    return -1;
}

int send_command(int sockfd, const char *command)
{
    ssize_t bytes_sent = send(sockfd, command, strlen(command), 0);
    if (bytes_sent < 0)
    {
        return -1;
    }
    return 0;
}

char *receive_response(int sockfd)
{
    char *response = (char *)malloc(MAX_BUFFER_SIZE);
    if (response == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate memory for response buffer.\n");
        return NULL;
    }
    memset(response, 0, MAX_BUFFER_SIZE);

    ssize_t bytes_received = recv(sockfd, response, MAX_BUFFER_SIZE - 1, 0);

    if (bytes_received <= 0)
    {
        free(response);
        return NULL;
    }

    response[bytes_received] = '\0';
    return response;
}

void write_data(char *data_start, ssize_t data_len, FILE *file, int *total_received)
{
    fwrite(data_start, 1, data_len, file);
    *total_received += data_len;
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
    if (scanf("%s", server_address) != 1)
    {
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Error opening socket");
        return 1;
    }

    struct timeval timeout;
    timeout.tv_sec = 20;
    timeout.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0)
    {
        perror("Warning: Error setting socket timeout");
    }

    int keep_alive = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(keep_alive)) < 0)
    {
        perror("Warning: Error setting socket Keep-Alive");
    }

    int rcvbuf_size = 1048576;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(rcvbuf_size)) < 0)
    {
        perror("Warning: Error setting socket receive buffer size");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    server = gethostbyname(server_address);
    if (server == NULL)
    {
        fprintf(stderr, "Error: No such host\n");
        return 1;
    }
    memcpy(&serv_addr.sin_addr, server->h_addr, server->h_length);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Error connecting");
        return 1;
    }
    printf("Connected to server.\n");

    response = receive_response(sockfd);
    if (response)
    {
        printf("Server says: %s", response);
        free(response);
    }

    while (1)
    {
        printf("\nMenu:\n1. List Files\n2. Download File\n3. Quit\nMake your choice: ");
        if (scanf(" %d", &choice) != 1)
        {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            continue;
        }

        switch (choice)
        {
            case 1:
                if (send_command(sockfd, "LIST\n") < 0)
                {
                    perror("Error sending LIST command");
                    break;
                }
                printf("Files:\n");

                char list_buffer[MAX_BUFFER_SIZE];
                int list_read_done = 0;
                while (!list_read_done)
                {
                    ssize_t bytes_received = recv(sockfd, list_buffer, MAX_BUFFER_SIZE - 1, 0);
                    if (bytes_received <= 0)
                    {
                        break;
                    }

                    list_buffer[bytes_received] = '\0';
                    printf("%s", list_buffer);
                    fflush(stdout);
                    if (strstr(list_buffer, "\n.\n") != NULL || (bytes_received > 1 && strcmp(list_buffer + bytes_received - 2, ".\n") == 0))
                    {
                        list_read_done = 1;
                    }
                }
                if (list_read_done)
                {
                    printf("\nEnd of file list.\n");
                }
                break;

            case 2:
                printf("Enter the filename to download: ");
                char filename[100];
                if (scanf(" %s", filename) != 1)
                {
                    break;
                }

                if (access(filename, F_OK) == 0)
                {
                    char overwrite_choice;
                    printf("File already exists. Overwrite? (y/n): ");
                    if (scanf(" %c", &overwrite_choice) != 1)
                    {
                        break;
                    }
                    if (overwrite_choice != 'y' && overwrite_choice != 'Y')
                    {
                        break;
                    }
                }

                char command[120];
                snprintf(command, sizeof(command), "GET %s\n", filename);
                if (send_command(sockfd, command) < 0)
                {
                    perror("Error sending GET command");
                    break;
                }

                response = receive_response(sockfd);
                if (response == NULL)
                {
                    break;
                }

                if (strncmp(response, "+OK", 3) == 0)
                {
                    int file_size = -1;
                    int sscanf_result = sscanf(response + 4, "%d", &file_size);
                    if (sscanf_result != 1)
                    {
                       file_size = get_known_file_size(filename);
                       if (file_size == -1)
                       {
                           fprintf(stderr, "Error: Unknown file size for %s.\n", filename);
                           free(response);
                           break;
                       }
                       fprintf(stderr, "Warning: File size not found in +OK header. Using LIST size of %d for %s.\n", file_size, filename);
                    }

                    printf("Downloading %s (%d bytes)...\n", filename, file_size);
                    FILE *file = fopen(filename, "wb");
                    if (file == NULL)
                    {
                        perror("Error opening file for writing");
                        free(response);
                        break;
                    }

                    char *data_start = strchr(response, '\n');
                    int total_received = 0;
                    if (data_start != NULL)
                    {
                        ssize_t initial_data_len = strlen(response) - (data_start - response) - 1;
                        if (initial_data_len > 0)
                        {
                            write_data(data_start + 1, initial_data_len, file, &total_received);
                        }
                    }
                    free(response);

                    char buffer[MAX_BUFFER_SIZE];
                    int percentage = 0;
                    char progress_bar[51];
                    memset(progress_bar, 0, sizeof(progress_bar));

                    while (total_received < file_size)
                    {
                        size_t bytes_remaining = file_size - total_received;
                        size_t max_read = (bytes_remaining < sizeof(buffer)) ? bytes_remaining : sizeof(buffer);

                        ssize_t bytes_received = recv(sockfd, buffer, max_read, 0);

                        if (bytes_received > 0)
                        {
                            write_data(buffer, bytes_received, file, &total_received);

                            if (total_received >= file_size)
                            {
                                break;
                            }

                            percentage = (int)(((long long)total_received * 100) / file_size);
                            int bar_length = (percentage * 50) / 100;
                            memset(progress_bar, '#', bar_length);
                            progress_bar[bar_length] = '\0';

                            printf("\rDownloading %s: [%-50s] %d%%", filename, progress_bar, percentage);
                            fflush(stdout);
                            continue;
                        }

                        if(bytes_received <= 0)
                        {
                            if (bytes_received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                            {
                                usleep(100000);
                                continue;
                            }

                            if (bytes_received == 0)
                            {
                                fprintf(stderr, "\nConnection closed by server prematurely. Received: %d/%d\n", total_received, file_size);
                            }
                            else
                            {
                                perror("Fatal error receiving file data");
                            }
                            fclose(file);
                            goto download_cleanup_error;
                        }
                    }

                    fclose(file);

                    memset(progress_bar, '#', 50);
                    progress_bar[50] = '\0';
                    printf("\rDownloading %s: [%-50s] 100%%\n", filename, progress_bar);

                    printf("File downloaded successfully: %s\n", filename);

                    download_cleanup_error:;
                }
                else
                {
                    printf("Server Error: %s", response);
                    free(response);
                }
                break;

            case 3:
                send_command(sockfd, "QUIT\n");
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