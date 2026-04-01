#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <limits.h>

#define PORT 8080
#define FILE_CHUNK_SIZE (64 * 1024)
#define REQUEST_BUFFER_SIZE (8 * 1024)

void handleClient(int clientfd);
void sendFile(int clientfd, char *file);
char *getFileType(char *path);

int main(int argc, char *argv[])
{
    bool autoOpenBrowser = true;

    for (int i = 1; i < argc; i++) {
	    if (strcmp(argv[i], "--nobrowser") == 0 || strcmp(argv[i], "-n") == 0) {
	        autoOpenBrowser = false;
	    }
    }

    printf("Server running at: http://localhost:%d/\n", PORT);

    int serverfd, clientfd;
    struct sockaddr_in serveraddr, clientaddr;
    socklen_t clientfdLength = sizeof(clientaddr);

    serverfd = socket(AF_INET, SOCK_STREAM, 0);

	int opt = 1;

    if (serverfd < 0)
    {
        printf("Socket creation failed\n");
        exit(1);
    }

    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (autoOpenBrowser) {
        char xdgCommand[64];
        snprintf(xdgCommand, sizeof(xdgCommand), "xdg-open http://localhost:%d/", PORT);
        system(xdgCommand);
    }

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(PORT);

    if (bind(serverfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    {
        printf("Bind failed\n");
        close(serverfd);
        exit(1);
    }

    if (listen(serverfd, 6) < 0)
    {
        printf("Listen failed\n");
        close(serverfd);
        exit(1);
    }

    while (1)
    {
        clientfd = accept(serverfd, (struct sockaddr *)&clientaddr, &clientfdLength);

        if (clientfd < 0)
        {
            printf("Accept failed\n");
            close(serverfd);
            exit(1);
        }

        printf("Connected\n");

        handleClient(clientfd);

        printf("Disconnected\n");
    }

    close(serverfd);

    return 0;
}

void handleClient(int clientfd)
{
    char *method, *path, *protocol;
    char request[REQUEST_BUFFER_SIZE];

    ssize_t bytesRead = read(clientfd, request, sizeof(request));

    if (bytesRead < 0)
    {
        printf("Read failed\n");
        close(clientfd);
        exit(1);
    }

    printf("Client request = %s\n", request);

    method = strtok(request, " ");
    path = strtok(NULL, " ");
    protocol = strtok(NULL, " ");

    printf("Method = %s\n", method);
    printf("Path = %s\n", path);

    if (strncmp(method, "GET", 3) != 0)
    {
        printf("Unsupported Method\n");

        char response[PATH_MAX];
        sprintf(response, "HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><body><h1>405 Method Not Allowed</h1></body></html>");
        write(clientfd, response, strlen(response));
        close(clientfd);
        return;
    }

    char resolved_path[PATH_MAX];
    if (path == NULL || strcmp(path, "/") == 0)
    {
        strncpy(resolved_path, "/index.html", sizeof(resolved_path));
    }
    else
    {
        strncpy(resolved_path, path, sizeof(resolved_path));
    }

    // TODO address URL encoding and edge cases for this exploit
    if (strstr(path, "..") != NULL)
    {
        char *forbidden = "HTTP/1.1 403 Forbidden\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><body><h1>403 Forbidden</h1></body></html>";
        write(clientfd, forbidden, strlen(forbidden));
        close(clientfd);
        return;
    }

    char root[PATH_MAX];
    realpath("www", root);

    char full[PATH_MAX];
    snprintf(full, sizeof(full), "www%s", resolved_path);

    char resolved[PATH_MAX];
    if (realpath(full, resolved) == NULL)
    {
        sendFile(clientfd, path);
        return;
    }

    if (strncmp(resolved, root, strlen(root)) != 0)
    {
        char *forbidden = "HTTP/1.1 403 Forbidden\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><body><h1>403 Forbidden</h1></body></html>";
        write(clientfd, forbidden, strlen(forbidden));
        close(clientfd);
        return;
    }

    sendFile(clientfd, resolved);
}

void sendFile(int clientfd, char *path)
{
    FILE *filePath;
    char *fileType;
    char file_buffer[FILE_CHUNK_SIZE];
    char response[4096];

    filePath = fopen(path, "rb");

    if (filePath == NULL)
    {
        // TODO filePath = fopen("404.html", "r");

        if (filePath == NULL)
        {
            snprintf(response, sizeof(response),
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: close\r\n\r\n"
                    "<html><body><h1>404 Not Found</h1></body></html>");

            write(clientfd, response, strlen(response));
			printf("Server response %s\n", response);
    		close(clientfd);
    		return;
        }
        else
        {
            snprintf(response, sizeof(response),
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: close\r\n\r\n");

            write(clientfd, response, strlen(response));

            while (fgets(file_buffer, sizeof(file_buffer), filePath) != NULL)
            {
                write(clientfd, file_buffer, strlen(file_buffer));
            }
        }
    }
    else
    {
        fileType = getFileType(path);

        if (fileType == NULL)
        {
            fileType = "application/octet-stream";
        }

        snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: %s\r\n"
                "Connection: close\r\n\r\n",
                fileType);

        write(clientfd, response, strlen(response));

        // TODO proper write loop

        size_t bytes;
        while ((bytes = fread(file_buffer, 1, sizeof(file_buffer), filePath)) > 0)
        {
            write(clientfd, file_buffer, bytes);
        }
    }

    printf("Server response %s\n", response);

    fclose(filePath);

    close(clientfd);
}

char *getFileType(char *path)
{
    char *ext = strrchr(path, '.');
    if (ext == NULL) return "application/octet-stream";

    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css")  == 0) return "text/css";
    if (strcmp(ext, ".js")   == 0) return "application/javascript";
    if (strcmp(ext, ".jpg")  == 0) return "image/jpeg";
    if (strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png")  == 0) return "image/png";
    if (strcmp(ext, ".mp4")  == 0) return "video/mp4";
    if (strcmp(ext, ".mp3")  == 0) return "audio/mp3";
    if (strcmp(ext, ".pdf")  == 0) return "application/pdf";
    if (strcmp(ext, ".ico")  == 0) return "image/x-icon";

    return "application/octet-stream";
}
