#include "../include/RedisCommandHandler.h"

#include <vector>
#include <sstream>
#include <algorithm>
#include <iostream> //debug

// RESP parser:
// *2\r\n$4\r\n\PING\r\n\n$4\r\nTEST\r\n
// *2 -> array of 2 elements
// $4 -> next string has 4 characters
// PING
// TEST

std::vector<std::string> parseRespCommand(const std::string &input){
    std::vector<std::string> tokens;
    if(input.empty()) return tokens;

    // If it doesnt starts woth '*', fallback to splitting by whiteSpaces
    if(input[0] != '*'){
        std::istringstream iss(input);
        std::string token;
        while(iss >> token ){
            tokens.push_back(token);
        }
        return tokens;
    }
    size_t pos=0;
    // Expect '*' followed by number of elements
    if(input[pos] != '*') return tokens;
    pos++; // skip '*'

    // crlf = Carriage Return (\r), Line Feed(\n)
    size_t crlf = input.find("\r\n", pos);
    if(crlf == std::string::npos) return tokens;

    int numElements = std::stoi(input.substr(pos, crlf - pos));
    pos = crlf +2;
    
    for (int i =0; i < numElements; i++){
        if(pos >= input.size() || input[pos] != '$') break; //format error
        pos++; // skip '$'

        crlf = input.find("\r\n", pos);
        if(crlf == std::string::npos) break;
        int len = std::stoi(input.substr(pos, crlf - pos));
        pos = crlf + 2;

        if(pos + len > input.size()) break;
        std::string token = input.substr(pos, len);
        tokens.push_back(token);
        pos += len+2; // skips token and CRLF
    }
    return tokens;
}

RedisCommandHandler::RedisCommandHandler() {}

std::string RedisCommandHandler::processCommand(const std::string& commandLine){
    // Use RESP perser
    auto tokens = parseRespCommand(commandLine);
    if(tokens.empty()) return"-Error : empty command\r\n";

    std::cout<< commandLine << "\n";

    for(auto& t : tokens){
        std::cout<<t<<"\n";
    }
    
    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    std::ostringstream response;

    // connect to Database

    // Check commands

    return response.str();
}