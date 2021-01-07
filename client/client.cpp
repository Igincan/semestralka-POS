#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>

int main()
{
    int socket_fd, n;
    struct sockaddr_in server_address;

    char buffer[256];

    struct hostent* server = gethostbyname("localhost");
    if (server == nullptr)
    {
        std::cerr << "Error, no such host." << std::endl;
        return 1;
    }

    memset(reinterpret_cast<char*>(&server_address), 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    memmove(
        reinterpret_cast<char*>(&server_address.sin_addr.s_addr),
        reinterpret_cast<char*>(server->h_addr),
        server->h_length
    );
    server_address.sin_port = htons(1500);

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        perror("Error creating socket.");
        return 2;
    }

    std::cout << "Connecting..." << std::endl;

    if (connect(socket_fd, reinterpret_cast<struct sockaddr*>(&server_address), sizeof(server_address)) == -1)
    {
        perror("Error connecting to socket.");
        return 3;
    }

    while (std::string(buffer) != "exit\n")
    {
        memset(buffer, 0, 256);
        std::cout << "Client: ";
        fgets(buffer, 255, stdin);

        n = write(socket_fd, buffer, strlen(buffer));
        if (n == -1)
        {
            perror("Error writing to socket.");
            return 4;
        }

        memset(buffer, 0, 256);
        n = read(socket_fd, buffer, 255);
        if (n == -1)
        {
            perror("Error reading from socket.");
            return 5;
        }

        std::cout << "Server: " << buffer;
    }

    close(socket_fd);
    
    return 0;
}