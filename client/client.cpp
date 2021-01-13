#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <mutex>
#include "getch.h"

struct input_data
{
    char last_pressed_key;
    std::mutex mutex;
};

int main(int argc, char const* argv[])
{

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " [host_name]" << std::endl;
        return 6;
    }
    const char* host_name = argv[1];

    int socket_fd, n;
    struct sockaddr_in server_address;

    char buffer[256];

    struct hostent* server = gethostbyname(host_name);
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

    std::cout << "Connected!" << std::endl;

    std::cout << "Waiting for other clients to connect..." << std::endl;

    memset(buffer, 0, 256);
    n = read(socket_fd, buffer, 255);
    if (std::string(buffer) != "all_clients_connected")
    {
        std::cerr << "Invalid response from server." << std::endl;
        return 5;
    }

    std::cout << "All clients connected!" << std::endl;

    unsigned fieldSize = 20;
    char field[fieldSize][fieldSize];

    struct input_data input_data =
    {
        ' '
    };

    std::thread input_thread([] (struct input_data* thread_data) {
        char input;
        while (input != 'q')
        {
            input = getch();
            {
                std::unique_lock<std::mutex> lock(thread_data->mutex);
                thread_data->last_pressed_key = input;
            }

        }
    }, &input_data);

    char last_pressed_key;

    while(last_pressed_key != 'q')
    {
        // write
        {
            std::unique_lock<std::mutex> lock(input_data.mutex);
            last_pressed_key = input_data.last_pressed_key;
        }
        n = write(socket_fd, &last_pressed_key, sizeof(last_pressed_key));
        if (n == -1)
        {
            perror("Error writing to socket.");
            return 4;
        }
        {
            std::unique_lock<std::mutex> lock(input_data.mutex);
            input_data.last_pressed_key = ' ';
        }

        // read
        memset(field, 0, fieldSize * fieldSize);
        n = read(socket_fd, field, fieldSize * fieldSize);

        // handling data
        std::cout << std::endl << std::endl << std::endl << std::endl;

        std::cout << " -";                             // top border gen.
        for (int i = 0; i < fieldSize*2 - 1; ++i) {
            std::cout << "-";
        }
        std::cout << std::endl;
        for (unsigned y = 0; y < fieldSize; y++)
        {
            std::cout << "|";
            for (unsigned x = 0; x < fieldSize; x++)
            {
                std::cout << field[x][y] << field[x][y];
            }
            std::cout << "|" << std::endl;
        }
        std::cout << " -";                          // bottom border gen.
        for (int i = 0; i < fieldSize*2 - 1; ++i) {
            std::cout << "-";
        }
        std::cout << std::endl;
    }

    input_thread.join();

    close(socket_fd);
    
    return 0;
}