#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

#define FIELD_SIZE 20

struct player_data
{
    char direction;
    struct
    {
        unsigned x;
        unsigned y;
    } head;
};

struct common_data
{
    unsigned clients_connected;
    char field[FIELD_SIZE][FIELD_SIZE];
    std::vector<struct player_data> players_data;

    std::mutex mutex;
    std::condition_variable all_clients_connected;
    std::condition_variable tick_passed;
};

struct client_data
{
    int client_socket_fd;
    unsigned player_index;

    struct common_data* common_data;
};

int main()
{
    srand(time(nullptr));

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

    unsigned field_size = 20;

    struct common_data common_data =
    {
        0      // clients_connected
        // field is initialized implicitly
        // players_data is initialized implicitly
        // for mutex and condition variables are used implicit default constructors
    };

    struct client_data client_datas[2];
    for (unsigned i = 0; i < 2; i++)
    {
        client_datas[i] =
        {
            0,
            i,
            &common_data
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
            std::unique_lock<std::mutex> lock(common_data.mutex);
            common_data.clients_connected++;
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
            if (n == -1)
            {
                perror("Error writing to socket.");
                return 5;
            }

            char input;

            while (input != 'q')
            {
                // write
                {
                    std::unique_lock<std::mutex> lock(thread_data->common_data->mutex);
                    n = write(thread_data->client_socket_fd, thread_data->common_data->field, pow(FIELD_SIZE, 2));
                }
                if (n == -1)
                {
                    perror("Error writing to socket.");
                    return 5;
                }

                // read
                n = read(thread_data->client_socket_fd, &input, sizeof(input));
                if (n == -1)
                {
                    perror("Error reading from socket.");
                    return 4;
                }
                if (input == 'w' || input == 'a' || input == 's' || input == 'd' || input == 'q')
                {
                    // handling input
                    if (input != 'q')
                    {
                        std::unique_lock<std::mutex> lock(thread_data->common_data->mutex);
                        thread_data->common_data->players_data[thread_data->player_index].direction = input;
                    }
                    else
                    {
                        std::cout << "client[" << thread_data->client_socket_fd << "]: " << "quitting" << std::endl;
                    }
                }

                {
                    std::unique_lock<std::mutex> lock(thread_data->common_data->mutex);
                    thread_data->common_data->tick_passed.wait(lock);
                }
            }

            close(thread_data->client_socket_fd);

            {
                std::unique_lock<std::mutex> lock(thread_data->common_data->mutex);
                thread_data->common_data->clients_connected--;
            }

        }, &client_datas[client_threads.size()]));

        std::cout << "Client connected (" << client_threads.size() << "/2)" << std::endl;
    }

    {
        std::unique_lock<std::mutex> lock(common_data.mutex);
        common_data.all_clients_connected.notify_all();
    }
    std::cout << "All clients connected!" << std::endl;

    // game logic
    // initialization

    {
        std::unique_lock<std::mutex> lock(common_data.mutex);
        common_data.players_data.push_back(
        {
            's',
            { 4, 4 }
        });
        common_data.players_data.push_back(
        {
            'w',
            { 15, 15 }
        });
        memset(common_data.field, '.', pow(FIELD_SIZE, 2));
        for (unsigned i = 0; i < common_data.players_data.size(); i++)
        {
            common_data.field[common_data.players_data[i].head.x][common_data.players_data[i].head.y] = 49 + i;
        }
    }

    // main cycle
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        {
            std::unique_lock<std::mutex> lock(common_data.mutex);

            for (unsigned i = 0; i < common_data.players_data.size(); i++)
            {
                struct player_data& player_data = common_data.players_data[i];
                common_data.field[player_data.head.x][player_data.head.y] = '.';
                switch (player_data.direction)
                {
                case 'w':
                    if (player_data.head.y != 0)
                    {
                        player_data.head.y--;
                    }
                    else
                    {
                        player_data.head.y = FIELD_SIZE - 1;
                    }
                    break;
                case 'a':
                    if (player_data.head.x != 0)
                    {
                        player_data.head.x--;
                    }
                    else
                    {
                        player_data.head.x = FIELD_SIZE - 1;
                    }
                    break;
                case 's':
                    if (player_data.head.y != FIELD_SIZE - 1)
                    {
                        player_data.head.y++;
                    }
                    else
                    {
                        player_data.head.y = 0;
                    }
                    break;
                case 'd':
                    if (player_data.head.x != FIELD_SIZE - 1)
                    {
                        player_data.head.x++;
                    }
                    else
                    {
                        player_data.head.x = 0;
                    }
                    break;
                }
                common_data.field[player_data.head.x][player_data.head.y] = 49 + i;
            }


            common_data.tick_passed.notify_all();
            if (common_data.clients_connected == 0)
            {
                break;
            }
        }
    }


    for (std::thread& thread : client_threads)
    {
        thread.join();
    }

    close(socket_fd);

    return 0;
}