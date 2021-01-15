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
#include <queue>

#define FIELD_SIZE 20

struct coordinates
{
    unsigned x;
    unsigned y;
};


struct player_data
{
    bool hasLost;
    char direction;
    struct coordinates head;
    std::queue<struct coordinates> body;
};

struct common_data
{
    unsigned clients_connected;
    unsigned number_of_players;
    unsigned inputs_handled;
    unsigned snakes_processed;
    bool tick_has_passed;
    char field[FIELD_SIZE][FIELD_SIZE];
    std::vector<struct player_data> players_data;

    std::mutex mutex;
    std::condition_variable all_clients_connected;
    std::condition_variable tick_passed;
    std::condition_variable all_inputs_handled;
    std::condition_variable all_snakes_processed;
};

struct client_data
{
    int client_socket_fd;
    unsigned player_index;

    struct common_data* common_data;
};

char get_opposite_direction(char& direction)
{
    switch (direction)
    {
    case 'w':
        return 's';
     case 'a':
        return 'd';
     case 's':
        return 'w';
     case 'd':
        return 'a';
    }
}

int main(int argc, char const* argv[])
{
    srand(time(nullptr));

    unsigned number_of_players;
    try
    {
        number_of_players = std::stoul(argv[1]);
    }
    catch(std::exception& e)
    {
        std::cerr << "Usage: " << argv[0] << " [number_of_players]" << std::endl;
        return 6;
    }

    if (number_of_players > 10)
    {
        number_of_players = 10;
    }
    else if (number_of_players < 1)
    {
        number_of_players = 1;
    }

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

    listen(socket_fd, number_of_players);

    std::cout << "Server is running." << std::endl;
    std::cout << "Waiting on clients to connect. (0/" << number_of_players << ")" << std::endl;

    struct common_data common_data =
    {
        0, // clients_connected
        number_of_players, // number_of_players
        0, // inputs_handled
        0, // snakes_processed
        false // tick_has_passed
        // field is initialized implicitly
        // players_data is initialized implicitly
        // for mutex and condition variables are used implicit default constructors
    };

    struct client_data client_datas[number_of_players];
    for (unsigned i = 0; i < number_of_players; i++)
    {
        client_datas[i] =
        {
            0,
            i,
            &common_data
        };
    }

    while (client_threads.size() < number_of_players)
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
                while (thread_data->common_data->clients_connected < thread_data->common_data->number_of_players)
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
                {
                    std::unique_lock<std::mutex> lock(thread_data->common_data->mutex);
                    thread_data->common_data->tick_has_passed = false;
                    while (!thread_data->common_data->tick_has_passed)
                    {
                        thread_data->common_data->tick_passed.wait(lock);
                    }
                }
                
                // read
                n = read(thread_data->client_socket_fd, &input, sizeof(input));
                if (n == -1)
                {
                    perror("Error reading from socket.");
                    return 4;
                }
                if (input >= 'a' && input <= 'z')
                {
                    // handling input
                    if (input == 'w' || input == 'a' || input == 's' || input == 'd')
                    {
                        {
                            std::unique_lock<std::mutex> lock(thread_data->common_data->mutex);
                            char& old_direction = thread_data->common_data->players_data[thread_data->player_index].direction;
                            if (get_opposite_direction(input) != old_direction)
                            {
                                old_direction = input;
                            }
                        }
                        std::cout << '[' << thread_data->player_index + 1 << "]: <" << input << ">" << std::endl;
                    }
                    else if (input == 'q')
                    {
                        std::cout << '[' << thread_data->player_index + 1 << "]: quitting" << std::endl;
                    }
                    else
                    {
                        std::cout << '[' << thread_data->player_index + 1 << "]: " << input << std::endl;
                    }
                }

                {
                    std::unique_lock<std::mutex> lock(thread_data->common_data->mutex);
                    if (++thread_data->common_data->inputs_handled == thread_data->common_data->number_of_players)
                    {
                        thread_data->common_data->all_inputs_handled.notify_one();
                    }
                    while (thread_data->common_data->snakes_processed < thread_data->common_data->number_of_players)
                    {
                        thread_data->common_data->all_snakes_processed.wait(lock);
                    }
                }

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
            }

            close(thread_data->client_socket_fd);

            {
                std::unique_lock<std::mutex> lock(thread_data->common_data->mutex);
                thread_data->common_data->clients_connected--;
            }

        }, &client_datas[client_threads.size()]));

        std::cout << "Client connected (" << client_threads.size() << '/' << number_of_players << ')' << std::endl;
    }

    {
        std::unique_lock<std::mutex> lock(common_data.mutex);
        common_data.all_clients_connected.notify_all();
    }
    std::cout << "All clients connected!" << std::endl;

    // game logic
    // initialization

    struct coordinates food =
    {
        rand() % FIELD_SIZE,
        rand() % FIELD_SIZE    
    };

    {
        std::unique_lock<std::mutex> lock(common_data.mutex);
        memset(common_data.field, ' ', pow(FIELD_SIZE, 2));
        common_data.field[food.x][food.y] = 'X';

        for (unsigned i = 0; i < number_of_players; i++)
        {
            char direction;
            switch (rand() % 4)
            {
            case 0:
                direction = 'w';
                break;
            case 1:
                direction = 'a';
                break;
            case 2:
                direction = 's';
                break;
            case 3:
                direction = 'd';
                break;
            }
            struct coordinates coords;
            do
            {
                coords =
                {
                    rand() % FIELD_SIZE,
                    rand() % FIELD_SIZE
                };
            } while (common_data.field[coords.x][coords.y] != ' ');
            
            common_data.players_data.push_back(
            {
                false,
                direction,
                coords
            });

            common_data.field[coords.x][coords.y] = 49 + i;
        }
    }

    // main cycle
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        {
            std::unique_lock<std::mutex> lock(common_data.mutex);

            common_data.snakes_processed = 0;
            common_data.tick_has_passed = true;
            common_data.tick_passed.notify_all();
            while (common_data.inputs_handled < common_data.number_of_players)
            {
                common_data.all_inputs_handled.wait(lock);
            }
            common_data.inputs_handled = 0;

            for (unsigned i = 0; i < common_data.players_data.size(); i++)
            {
                struct player_data& player_data = common_data.players_data[i];
                if (player_data.hasLost)
                {
                    continue;
                }
                struct coordinates old_head = player_data.head;
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
                player_data.body.push(old_head);
                switch (common_data.field[player_data.head.x][player_data.head.y])
                {
                case 'X':
                    {
                        do
                        {
                            food =
                            {
                                rand() % FIELD_SIZE,
                                rand() % FIELD_SIZE
                            };
                        } while (common_data.field[food.x][food.y] != ' ');
                        common_data.field[food.x][food.y] = 'X';
                    }
                    break;
                case ' ':
                    {
                        struct coordinates tail = player_data.body.front();
                        common_data.field[tail.x][tail.y] = ' ';
                        player_data.body.pop();
                    }
                    break;
                default:
                    {
                        player_data.hasLost = true;
                        while (!player_data.body.empty())
                        {
                            struct coordinates body_part = player_data.body.front();
                            common_data.field[body_part.x][body_part.y] = ' ';
                            player_data.body.pop();
                        }
                    }
                    break;
                }
                if (!player_data.hasLost)
                {
                    common_data.field[player_data.head.x][player_data.head.y] = 49 + i;
                }
            }

            common_data.snakes_processed = common_data.number_of_players;
            common_data.all_snakes_processed.notify_all();

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