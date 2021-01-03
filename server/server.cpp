#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
    int socket_fd, new_socket_fd;
    struct sockaddr_in server_address, client_address;
    socklen_t client_length = sizeof(client_address);
    int n;
    char buffer[256];

    memset(reinterpret_cast<char*>(&server_address), 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(1500);

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        perror("Error creating socket.");
        return 1;
    }

    if (bind(socket_fd, reinterpret_cast<struct sockaddr*>(&server_address), sizeof(server_address)))
    {
        perror("Error binding socket address.");
        return 2;
    }

    listen(socket_fd, 5);

    std::cout << "Listening on port: 1500" << std::endl;

    new_socket_fd = accept(socket_fd, reinterpret_cast<struct sockaddr*>(&client_address), &client_length);
    if (new_socket_fd == -1)
    {
        perror("Error accepting socket.");
        return 3;
    }

    while (strcmp(buffer, "exit"))
    {
        memset(buffer, 0, 256);
        n = read(new_socket_fd, buffer, 255);
        if (n == -1)
        {
            perror("Error reading from socket.");
            return 4;
        }
        std::cout << "Client: " << buffer;

        memset(buffer, 0, 256);
        std::cout << "Server: ";
        fgets(buffer, 255, stdin);

        n = write(new_socket_fd, buffer, strlen(buffer) + 1);
        if (n == -1)
        {
            perror("Error writing socket.");
            return 5;
        }
    }


    close(new_socket_fd);
    close(socket_fd);

    return 0;
}