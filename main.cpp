#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>
#include "constante.h"

#define PORT 51515

void shot(std::map<int, Player>* clientPlayers,int x, int y, int angle, int id);

using json = nlohmann::json;

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    fd_set readfds;
    std::vector<int> client_sockets;
    std::map<int, Player> clientPlayers;

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    std::cout << "Server is listening on port " << PORT << std::endl;

    // Main loop
    while (true) {
        // Clear the socket set
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        // Add client sockets to set
        for (int client_socket : client_sockets) {
            FD_SET(client_socket, &readfds);
            if (client_socket > max_sd) {
                max_sd = client_socket;
            }
        }

        // Wait for an activity on one of the sockets
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("select error");
        }

        // If something happened on the master socket, it's an incoming connection
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            std::cout << "New connection, socket fd is " << new_socket << std::endl;

            // Add new socket to client_sockets vector
            client_sockets.push_back(new_socket);

            // Notify existing clients of the new connection
            for (int client_socket : client_sockets) {
                if (client_socket != new_socket) {
                    json shotData;
                    shotData["data"] = "AddPlayer";
                    shotData["id"] = client_socket;
                    shotData["position"]["x"] = clientPlayers[client_socket].posX;
                    shotData["position"]["y"] = clientPlayers[client_socket].posY;

                    std::string shotStr = shotData.dump();

                    if (send(new_socket, shotStr.c_str(), shotStr.length(), 0) < 0) {
                        perror("send");
                    }
                }
            }
        }

        // Read messages from clients and broadcast them
        for (int client_socket : client_sockets) {
            if (FD_ISSET(client_socket, &readfds)) {
                int valread = read(client_socket, buffer, 1024);
                if (valread == 0) {
                    // Client disconnected
                    std::cout << "Client disconnected, socket fd is " << client_socket << std::endl;
                    close(client_socket);
                    client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), client_socket), client_sockets.end());

                    for (int client_sock : client_sockets) {
                            json shotData;
                            shotData["data"] = "RemovePlayer";
                            shotData["id"] = client_socket;

                            std::string shotStr = shotData.dump();

                            if (send(client_sock, shotStr.c_str(), shotStr.length(), 0) < 0) {
                                perror("send");
                            }
                    }
                } else {
                    buffer[valread] = '\0';
                    std::cout << "Received message: " << buffer << " from socket fd: " << client_socket << std::endl;
                    std::string jsonStr(buffer, valread);

                    try {
                        // Deserialize the string into JSON object
                        json receivedData = json::parse(jsonStr);

                        if (receivedData["data"] == "init") {
                            clientPlayers[client_socket].posX = receivedData["position"]["x"];
                            clientPlayers[client_socket].posY = receivedData["position"]["y"];
                            clientPlayers[client_socket].pv = receivedData["pv"];

                            for (int other_socket: client_sockets) {
                                if (other_socket != client_socket) {
                                    json shotData;
                                    shotData["data"] = "AddPlayer";
                                    shotData["id"] = client_socket;
                                    shotData["position"]["x"] = clientPlayers[client_socket].posX;
                                    shotData["position"]["y"] = clientPlayers[client_socket].posY;

                                    std::string shotStr = shotData.dump();

                                    if (send(other_socket, shotStr.c_str(), shotStr.length(), 0) < 0) {
                                        perror("send");
                                    }
                                }
                            }
                        } else if (receivedData["data"] == "move") {
                            clientPlayers[client_socket].posX = receivedData["position"]["x"];
                            clientPlayers[client_socket].posY = receivedData["position"]["y"];

                            for (int other_socket: client_sockets) {
                                if (other_socket != client_socket) {
                                    json shotData;
                                    shotData["data"] = "MovePlayer";
                                    shotData["id"] = client_socket;
                                    shotData["position"]["x"] = clientPlayers[client_socket].posX;
                                    shotData["position"]["y"] = clientPlayers[client_socket].posY;

                                    std::string shotStr = shotData.dump();

                                    if (send(other_socket, shotStr.c_str(), shotStr.length(), 0) < 0) {
                                        perror("send");
                                    }
                                }
                            }
                        } else if (receivedData["data"] == "shot") {
                            shot(&clientPlayers, receivedData["position"]["x"], receivedData["position"]["y"],receivedData["angle"],client_socket);
                        }

                        else {
                                std::cerr << "Received JSON does not contain 'init' or 'move' or 'shot'" << std::endl;
                            }
                        }
                    catch (json::parse_error& e) {
                        std::cerr << "JSON parse error: " << e.what() << std::endl;
                    }
                }
            }
        }
    }
    close(server_fd);
    return 0;
}

void shot(std::map<int, Player>* clientPlayers,int posX, int posY, int angle, int id){
    const float tolerance = 5;
    for (auto& pair : *clientPlayers) {
        if (id != pair.first) {
            float distanceMonster = sqrt(pow(pair.second.posX - posX, 2) + pow(pair.second.posY - posY, 2));
            float x = posX + static_cast<int>(cos(angle * M_PI / 180) * distanceMonster);
            float y = posY + static_cast<int>(sin(angle * M_PI / 180) * distanceMonster);

            float distanceShot = sqrt(pow(x - pair.second.posX, 2) + pow(y - pair.second.posY, 2));

            if (distanceShot <= tolerance) {
                json shotData;
                shotData["data"] = "ShotPlayer";
                pair.second.pv += -20;
                shotData["pv"] = pair.second.pv;

                std::string shotStr = shotData.dump();

                if (send(pair.first, shotStr.c_str(), shotStr.length(), 0) < 0) {
                    perror("send");
                }
            }
        }
    }
}