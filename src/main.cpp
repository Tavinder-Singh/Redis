#include "../include/RedisServer.h"
#include "../include/RedisDatabase.h"
#include<iostream>
#include<thread>
#include<chrono>


int main(int argc, char* argv[]){
    int port = 6379; //default
    if(argc >= 2) port = std::stoi(argv[1]);

    if(RedisDatabase::getInstance().load("dump.my_rdb"))
        std::cout << "Database loaded dump.my_rdb\r\n";
    else
        std::cout << "No dump found or load failed, starting with empty DB\r\n";

    RedisServer server(port);

    // Background Persistance : Dumps the database every 300sec (5 * 60 save database)
    std::thread persistanceThread([](){
        while(true){
            std::this_thread::sleep_for(std::chrono::seconds(300));
            // Dump the database
            if(!RedisDatabase::getInstance().dump("dump.my_rdb"))
                std::cerr << "Error Dumping Database\n";
            else
                std::cout << "Database Dumped to dump.my_rdb\n";
        }
    });
    persistanceThread.detach();

    server.run();

    return 0;
}