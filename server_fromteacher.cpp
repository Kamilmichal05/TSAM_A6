//
// Simple chat server for TSAM
//
// Command line: ./server <port>
//
// Author(s):
//    Jacky Mallett (jacky@ru.is)
//    Stephan Schiffel (stephans@ru.is)

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
#include <list>
#include <poll.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <map>

#include <unistd.h>

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG  5          // Allowed length of queue of waiting connections

// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.


struct MessageFrom {
    std::string text;
    std::string from;
};

std::map<std::string,std::vector<MessageFrom>> mails;

class Client
{
  public:
    int sock;              // socket of client connection
    std::string name;           // Limit length of name of client's user
    std::string receive_Buffer;
    std::string send_Buffer;
    int length_read = 0;
    uint16_t message_length = 0;

    Client(int socket) : sock(socket){} 
    ~Client(){}            // Virtual destructor defined for base class
};

std::map<int, Client*> clients; // Lookup table for per Client information

// Open socket for specified port.
//
// Returns -1 if unable to create the socket for any reason.

int open_socket(int portno)
{
   struct sockaddr_in sk_addr;   // address settings for bind()
   int sock;                     // socket opened for this port
   int set = 1;                  // for setsockopt

   // Create socket for connection. Set to be non-blocking, so recv will
   // return immediately if there isn't anything waiting to be read.
#ifdef __APPLE__     
   if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("Failed to open socket");
      return(-1);
   }
#else
   if((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
   {
     perror("Failed to open socket");
    return(-1);
   }
#endif

   // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
   // program exit.

   if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
   {
      perror("Failed to set SO_REUSEADDR:");
   }
   set = 1;
#ifdef __APPLE__     
   if(setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
   {
     perror("Failed to set SOCK_NOBBLOCK");
   }
#endif
   memset(&sk_addr, 0, sizeof(sk_addr));

   sk_addr.sin_family      = AF_INET;
   sk_addr.sin_addr.s_addr = INADDR_ANY;
   sk_addr.sin_port        = htons(portno);

   // Bind to socket to listen for connections from clients

   if(bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
   {
      perror("Failed to bind to socket:");
      return(-1);
   }
   else
   {
      return(sock);
   }
}

// Close a client's connection, and remove it from the list of file
// descriptors being watched by poll().
//
// Note: this only removes the socket from the poll() watch list. It does
// NOT remove the corresponding entry from the "clients" map - callers are
// responsible for that (see the main loop below).

void closeClient(int clientSocket, std::vector<struct pollfd> *fds)
{
     printf("Client closed connection: %d\n", clientSocket);

     close(clientSocket);      

     // Find and remove this socket's entry from the list of fds polled.
     auto fdIt = std::find_if(fds->begin(), fds->end(),
                    [clientSocket](const struct pollfd &pfd)
                    {
                        return pfd.fd == clientSocket;
                    });

     if(fdIt != fds->end())
     {
         fds->erase(fdIt);
     }
}

// Send a message to a client. send() may not be able to accept the whole
// message at once, so keep going until everything has been written.
//
// Returns false if the client has gone away.

std::string building_message(std::string text) {

    size_t length = text.size() + 5;
    uint16_t totallen = htons(length);
    std::string frame;
    frame += "\x01";
    frame.append(reinterpret_cast<const char*>(&totallen), 2);
    frame += '\x02';
    frame += text;
    frame += '\x03';
    return frame;
}

bool sendMessage(int sock, std::string mytext)
{
    std::string frame = building_message(mytext);

    ssize_t left_bytes = send(sock,frame.data(), frame.size(),0);

    if (left_bytes == -1) {
        return false;
    }

    const char* data_location = frame.data();
    size_t bytes_remaining = frame.size();

    while(bytes_remaining < 0)
    {
        ssize_t n = send(sock, data_location, bytes_remaining, 0);

        if(n > 0)
        {
            data_location += n;
            bytes_remaining -= static_cast<size_t>(n);
        }
        else {
            return false;
        }
    }

   return true;
}

// Process command from client on the server

void clientCommand(int clientSocket, std::vector<struct pollfd> *fds,
                  const char *buffer) 
{

  std::vector<std::string> tokens;
  std::string token;

  // Split command from client into tokens for parsing
  std::stringstream stream(buffer);

  while (std::getline(stream, token, ','))
    tokens.push_back(token);

  if((tokens[0].compare("SENDMSG") == 0) && (tokens.size() == 2))
  {
    std::string from = tokens[1];
    std::string to = tokens[2];
    std::string body = tokens[3]; 
    std::string message;

    if (tokens.size() < 4) {
        return;
    }

    else {

        for (size_t i = 4; i < tokens.size(); i++)
        {
            body+= "," + tokens[i];
        }
    }

    mails[to].push_back(MessageFrom{body, from});
    std::string status = "successful";
    sendMessage(clientSocket,status);
  }
  else if(tokens[0].compare("GETMSG") == 0)
  {

    std::string name = tokens[1];
    auto mail = mails.find(name);

    if (mail->second.empty() || mail == mails.end()) {
        return;
    }

    std::string singe_message = mail->second.front().text;
    mail->second.erase(mail->second.begin());
    sendMessage(clientSocket,singe_message);


  }
  else if(tokens[0].compare("STATUSREQ") == 0)
  {
     
    int count = 0;
    int empty = 0;
    std::string temp;
    for (auto data : mails) {

        if (data.second.empty()) {
            continue;
        }
        else {
            empty = 1;
        }

        size_t our_size = data.second.size();
        std::string name = data.first;
        temp += name;
        temp += ":";
        temp += std::to_string(our_size);
        temp += ",";
        count++;
    }

    if (empty == 0) {
        temp = "EMPTY";
    }

    if (count > 0 && temp != "EMPTY") {
        temp.pop_back();
    }
    sendMessage(clientSocket,temp);
}

 
  
  else
  {
      std::cout << "Unknown command from client:" << buffer << std::endl;
  }
     
}

int main(int argc, char* argv[])
{
    bool finished;
    int listenSock;                 // Socket for connections to server
    int clientSock;                 // Socket of connecting client

    // List of file descriptors being watched by poll(). By convention,
    // index 0 is always the listening socket; all other entries are
    // connected clients.
    std::vector<struct pollfd> fds;

    struct sockaddr_in client;
    socklen_t clientLen;
    char buffer[1025];              // buffer for reading from clients

    if(argc != 2)
    {
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }

    // Setup socket for server to listen to

    listenSock = open_socket(atoi(argv[1])); 
    printf("Listening on port: %d\n", atoi(argv[1]));

    if(listen(listenSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", argv[1]);
        exit(0);
    }
    else 
    // Add listen socket to the list of fds being polled.
    {
        fds.push_back({listenSock, POLLIN, 0});
    }

    finished = false;

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
               clientSock = accept(listenSock, (struct sockaddr *)&client,
                                   &clientLen);
               printf("accept***\n");
               // Add new client to the list of fds being polled
               fds.push_back({clientSock, POLLIN, 0});

               // create a new client to store information.
               // Re-use the record for this socket if we have seen it
               // before, so we don't leak a Client every time someone
               // disconnects and reconnects.
               if(clients.find(clientSock) == clients.end())
               {
                  clients[clientSock] = new Client(clientSock);
               }

               // Decrement the number of sockets waiting to be dealt with

               printf("Client connected on server: %d\n", clientSock);
            }
            // Now check for commands from clients
            std::list<Client *> disconnectedClients;  
            
               for(size_t i = 1; i < readyFds.size(); i++)
               {
                  if(readyFds[i].revents & POLLIN)
                  {
                      int sock = readyFds[i].fd;
                      Client *client = clients[sock];
                      ssize_t bytes_received = recv(sock, buffer, sizeof(buffer), MSG_DONTWAIT);

                      // recv() == 0 means client has closed connection
                      if(bytes_received == 0)
                      {
                          disconnectedClients.push_back(client);
                          closeClient(sock, &fds);

                      }

                      else if (bytes_received == -1) {
                        closeClient(sock,&fds);
                      }
                      // We don't check for -1 (nothing received) because poll()
                      // only triggers if there is something on the socket for us.
                      else
                      {

                        client->receive_Buffer.append(buffer, bytes_received);

                        if (client->receive_Buffer.size() >= 3 && client->length_read == 0) {
                            uint16_t netLen;
                            memcpy(&netLen, client->receive_Buffer.data() + 1, 2);
                            client->message_length = ntohs(netLen);
                            client->length_read = 1;
                        }

                        if (client->length_read == 1 && client->receive_Buffer.size() >= client->message_length) {
                            
                            
                            std::string new_command = client->receive_Buffer.substr(4, client->message_length - 5);
                            client->receive_Buffer.erase(0, client->message_length);
                            client->message_length = 0;
                            client->length_read = 0;

                            if (client->message_length > 5005) {
                                continue;
                            }
                            else {
                                clientCommand(sock, &fds, new_command.data());
                            }
                            
                        }
                      }
                  }
               }
               // Remove client from the clients list
               for(auto const& c : disconnectedClients)
                  clients.erase(c->sock);
        }
    }
}
