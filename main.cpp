#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "constante.h"

#define PORT 8080

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


    // Créer le socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Attacher le socket au port 8080
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

    // Boucle principale
    while (true) {
        // Réinitialiser le set de descripteurs de fichier
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        // Ajouter les sockets clients au set de descripteurs de fichier
        for (int client_socket : client_sockets) {
            FD_SET(client_socket, &readfds);
            if (client_socket > max_sd) {
                max_sd = client_socket;
            }
        }

        // Si quelque chose se passe sur le socket maître, c'est une nouvelle connexion
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            for (int client_socket : client_sockets)
            std::cout << "New connection, socket fd is " << new_socket << std::endl;
            for (int client_socket : client_sockets){
                json shotData;
                shotData["data"] = "AddPlayer";
                shotData["id"] = client_socket;
                shotData["position"]["x"] = clientPlayers[client_socket].posX;
                shotData["position"]["y"] = clientPlayers[client_socket].posY;

                std::string shotStr = shotData.dump();

                if (send(client_socket, shotStr.c_str(), shotStr.length(), 0) < 0) {
                    perror("send");
                }
            }
            client_sockets.push_back(new_socket);
        }

        // Lire les messages des clients et les retransmettre à tous les autres clients
        for (int client_socket : client_sockets) {
            if (FD_ISSET(client_socket, &readfds)) {
                int valread = read(client_socket, buffer, 1024);
                if (valread == 0) {
                    // Le client a fermé la connexion
                    std::cout << "Client disconnected, socket fd is " << client_socket << std::endl;
                    close(client_socket);
                    client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), client_socket), client_sockets.end());
                } else {
                    buffer[valread] = '\0';
                    std::cout << "Received message: " << buffer << " from socket fd: " << client_socket << std::endl;
                    std::string jsonStr(buffer, valread);

                    try {
                        // Désérialiser la chaîne en objet JSON
                        json receivedData = json::parse(jsonStr);
                        if (receivedData["data"] == "init"){
                            clientPlayers[client_socket].posX = receivedData["position"]["x"];
                            clientPlayers[client_socket].posY = receivedData["position"]["y"];
                            clientPlayers[client_socket].pv = receivedData["pv"];

                            for (int other_socket : client_sockets) {
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

                        } else {
                            std::cerr << "Received JSON does not contain 'init'" << std::endl;
                        }
                        if (receivedData["data"] == "move"){
                            clientPlayers[client_socket].posX = receivedData["position"]["x"];
                            clientPlayers[client_socket].posY = receivedData["position"]["y"];

                            for (int other_socket : client_sockets) {
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
                        } else {
                            std::cerr << "Received JSON does not contain 'init'" << std::endl;
                        }

                    } catch (json::parse_error& e) {
                        std::cerr << "JSON parse error: " << e.what() << std::endl;
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
