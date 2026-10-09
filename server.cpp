#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <poll.h>
#include <iostream>
#include <sstream>
#include <map>
#include <list>

int main(int argc, char* argv[]) {

    if (argc != 2) {
        std::cerr << "Wrong number of arguments" << std::endl;
        exit(1);
    }

    int port = atoi(argv[1]);
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creating socket");
        exit(1);
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    std::vector<struct pollfd> fds;

    if(bind(sockfd, (struct sockaddr *)&server_addr,
        sizeof(server_addr)) < 0)
    {
        perror("Failed to bind to socket:");
        return(-1);
    }

    printf("Listening on port: %d\n", atoi(argv[1]));

    if(listen(sockfd, 5) < 0)
    {
        printf("Listen failed on port %s\n", argv[1]);
        exit(0);
    }
    else 
    // Add listen socket to the list of fds being polled.
    {
        fds.push_back({sockfd, POLLIN, 0});
    }

    bool finished = false;
    char buffer[1025];
    int clientSock;
    socklen_t server_len;

    while(!finished)
    {
        memset(buffer, 0, sizeof(buffer));

        // Wait (indefinitely) until at least one of our sockets has
        // something to be read() on it.
        int n = poll(fds.data(), fds.size(), -1);

        if(n < 0)
        {
            perror("poll failed - closing down\n");
            finished = true;
        }
        else
        {
            // Take a snapshot of the current fds/revents before processing
            // them below, since fds itself may be modified while we go
            // (new clients connecting, existing clients disconnecting).
            std::vector<struct pollfd> readyFds = fds;

            // First, accept any new connections to the server on the listening socket
            if(readyFds[0].revents & POLLIN)
            {
                struct sockaddr_in client;
                socklen_t clientLen = sizeof(client);
                char buffer[1025];

               clientSock = accept(sockfd, (struct sockaddr *)&client,
                                   &clientLen);
               printf("accept***\n");
               // Add new client to the list of fds being polled
               fds.push_back({clientSock, POLLIN, 0});

               // create a new client to store information.
               // Re-use the record for this socket if we have seen it
               // before, so we don't leak a Client every time someone
               // disconnects and reconnects.
               //if(clients.find(clientSock) == clients.end())
               {
                  //clients[clientSock] = new Client(clientSock);
               }

               printf("Client connected on server: %d\n", clientSock);
            }
            // Now check for commands from clients
            //std::list<Client *> disconnectedClients;  
        
               for(size_t i = 1; i < readyFds.size(); i++)
               {
                  if(readyFds[i].revents & POLLIN)
                  {
                      int sock = readyFds[i].fd;
                      //Client *client = clients[sock];

                      // recv() == 0 means client has closed connection
                     if(recv(sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
                      {
                          //disconnectedClients.push_back(client);
                          //closeClient(sock, &fds);

                      }
                      // We don't check for -1 (nothing received) because poll()
                      // only triggers if there is something on the socket for us.
                      else
                      {
                          std::cout << buffer << std::endl;
                          //clientCommand(sock, &fds, buffer);
                      }
                  }
               }
               // Remove client from the clients list
               //for(auto const& c : disconnectedClients)
                  //clients.erase(c->sock);
        }
    }

}