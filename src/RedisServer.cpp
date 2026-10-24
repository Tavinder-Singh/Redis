#include "include/RedisServer.h"

#include <iostream>
#include <unistd.h>
#include <sys/socket.h>

using namespace std;
static RedisServer* globalServer = nullptr;

RedisServer::RedisServer(int port) : port(port), server_socket(-1), running(true){
    globalServer = this;
}

void RedisServer::shutdown(){
    running = false;
    if(server_socket != -1){
        close(server_socket);
    }
    cout<< "Server Shutdown Complete" << endl;
}

void RedisServer::run(){
    server_socket = socket(AF_INET, SOCK_STREAM,0);
    if(server_socket < 0){
        std::cerr << "Error Creating Server Socket\n";
        return;
    }
    int opt=1;
    setsockopt(server_socket,SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))
}