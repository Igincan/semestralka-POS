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
#include <mutex>
#include <condition_variable>
#include <vector>

struct client_common_data
{
    unsigned clients_connected;

    std::mutex mutex;
    std::condition_variable all_clients_connected;
};

struct client_data
{
    int client_socket_fd;

    struct client_common_data* common_data;
};

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

    std::cout << "Server is running." << std::endl;
    std::cout << "Waiting on clients to connect. (0/2)" << std::endl;

    struct client_common_data client_common_data =
    {
        0
        // for mutex and condition variables are used implicit default constructors
    };

    struct client_data client_datas[2];
    for (unsigned i = 0; i < 2; i++)
    {
        client_datas[i] =
        {
            0,
            &client_common_data
        };
    }

    while (client_threads.size() < 2)
    {
        new_socket_fd = accept(socket_fd, reinterpret_cast<struct sockaddr*>(&client_address), &client_length);
        if (new_socket_fd == -1)
        {
            perror("Error accepting socket.");
            return 3;
        }

        client_datas[client_threads.size()].client_socket_fd = new_socket_fd;
        {
            std::unique_lock<std::mutex> lock(client_common_data.mutex);
            client_common_data.clients_connected++;
        }

        client_threads.push_back(std::thread([] (struct client_data* thread_data) {

            int n;
            char buffer[256];

            {
                std::unique_lock<std::mutex> lock(thread_data->common_data->mutex);
                while (thread_data->common_data->clients_connected < 2)
                {
                    thread_data->common_data->all_clients_connected.wait(lock);
                }
            }
            memset(buffer, 0, 256);
            const char* msg = "all_clients_connected";
            n = write(thread_data->client_socket_fd, msg, strlen(msg) + 1);
            
            while (std::string(buffer) != "exit\n")
            {
                memset(buffer, 0, 256);
                n = read(thread_data->client_socket_fd, buffer, 255);
                if (n == -1)
                {
                    perror("Error reading from socket.");
                    return 4;
                }
                std::cout << "client[" << thread_data->client_socket_fd << "]: " << buffer;
            }

            close(thread_data->client_socket_fd);
        }, &client_datas[client_threads.size()]));

        std::cout << "Client connected (" << client_threads.size() << "/2)" << std::endl;
    }

    {
        std::unique_lock<std::mutex> lock(client_common_data.mutex);
        client_common_data.all_clients_connected.notify_all();
    }
    std::cout << "All clients connected!" << std::endl;


    for (std::thread& thread : client_threads)
    {
        thread.join();
    }

    close(socket_fd);

    return 0;
}