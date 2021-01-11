#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <vector>

int main()
{
    int socket_fd, new_socket_fd;
    struct sockaddr_in server_address, client_address;
    socklen_t client_length = sizeof(client_address);

    std::vector<std::thread> client_threads;

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

    while (client_threads.size() < 3)
    {
        new_socket_fd = accept(socket_fd, reinterpret_cast<struct sockaddr*>(&client_address), &client_length);
        if (new_socket_fd == -1)
        {
            perror("Error accepting socket.");
            return 3;
        }

        client_threads.push_back(std::thread([] (int thread_socket_fd) {

            int n;
            char buffer[256];

            while (std::string(buffer) != "exit\n")
            {
                memset(buffer, 0, 256);
                n = read(thread_socket_fd, buffer, 255);
                if (n == -1)
                {
                    perror("Error reading from socket.");
                    return 4;
                }
                std::cout << "Client[" << thread_socket_fd << "]: " << buffer;
            }

            close(thread_socket_fd);
        }, new_socket_fd));
    }


    for (std::thread& thread : client_threads)
    {
        thread.join();
    }

    close(socket_fd);

    return 0;
}