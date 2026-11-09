#include "../include/RedisCommandHandler.h"
#include "../include/RedisDatabase.h"

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

std::vector<std::string> parseRespCommand(const std::string &input)
{
    std::vector<std::string> tokens;
    if (input.empty())
        return tokens;

    // If it doesnt starts woth '*', fallback to splitting by whiteSpaces
    if (input[0] != '*')
    {
        std::istringstream iss(input);
        std::string token;
        while (iss >> token)
        {
            tokens.push_back(token);
        }
        return tokens;
    }
    size_t pos = 0;
    // Expect '*' followed by number of elements
    if (input[pos] != '*')
        return tokens;
    pos++; // skip '*'

    // crlf = Carriage Return (\r), Line Feed(\n)
    size_t crlf = input.find("\r\n", pos);
    if (crlf == std::string::npos)
        return tokens;

    int numElements = std::stoi(input.substr(pos, crlf - pos));
    pos = crlf + 2;

    for (int i = 0; i < numElements; i++)
    {
        if (pos >= input.size() || input[pos] != '$')
            break; // format error
        pos++;     // skip '$'

        crlf = input.find("\r\n", pos);
        if (crlf == std::string::npos)
            break;
        int len = std::stoi(input.substr(pos, crlf - pos));
        pos = crlf + 2;

        if (pos + len > input.size())
            break;
        std::string token = input.substr(pos, len);
        tokens.push_back(token);
        pos += len + 2; // skips token and CRLF
    }
    return tokens;
}

//---------------------------
// Common Commands
//---------------------------
static std::string handlePing(const std::vector<std::string> &tokens, RedisDatabase &db)
{
    return "+PONG\r\n";
}

static std::string handleEcho(const std::vector<std::string> &tokens, RedisDatabase &db)
{
    if (tokens.size() < 2)
        return "-Error ECHO requires a message\r\n";
    return "+" + tokens[1] + "\r\n";
}

static std::string handleFlushAll(const std::vector<std::string> &tokens, RedisDatabase &db)
{
    db.flushAll();
    return "+OK\r\n";
}

//---------------------------
// Key/Value Operations
//---------------------------
static std::string handleSet(const std::vector<std::string> &tokens, RedisDatabase &db){
    if (tokens.size() < 3)
        return "-Error: SET requires key and values\r\n";
    db.set(tokens[1], tokens[2]);
    return "+OK\r\n";
}

static std::string handleGet(const std::vector<std::string> &tokens, RedisDatabase &db){
    if (tokens.size() < 2)
        return "-Error: GET requires a key\r\n";
    std::string value;
    if (db.get(tokens[1], value))
        return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
    else
        return "$-1\r\n";
}

static std::string handleKeys(const std::vector<std::string> &tokens, RedisDatabase &db){
    std::vector<std::string> allKeys = db.keys();
    std::ostringstream oss;
    oss << "*" << allKeys.size() << "\r\n";
    for (const auto &key : allKeys)
        oss << "$" << key.size() << "\r\n"<< key << "\r\n";
    return oss.str();
}

static std::string handleType(const std::vector<std::string> &tokens, RedisDatabase &db){
        if (tokens.size() < 2)
            return "-Error: TYPE requires a key\r\n";
        else
            return "+" + db.type(tokens[1]) + "\r\n";
}

static std::string handleDel(const std::vector<std::string> &tokens, RedisDatabase &db, std::string cmd){
    if (tokens.size() < 2)
        return "-Error: " + cmd + " requires a key\r\n";
    bool res = db.del(tokens[1]);
    return ":" + std::to_string((res ? 1 : 0)) + "\r\n";    
}

static std::string handleExpire(const std::vector<std::string> &tokens, RedisDatabase &db){
    if(tokens.size() < 2)
        return "-Error: EXPIRE requires key and time in second\r\n";
    try{
        int seconds = stoi(tokens[2]);
        if(db.expire(tokens[1],seconds))
            return "+OK\r\n";
        else
            return "-Error: Key not found\r\n";
    }catch(const std::exception&){
        "-Error: Invalid expiration time\r\n";
    }
}

static std::string handleRename(const std::vector<std::string> &tokens, RedisDatabase &db){
    if (tokens.size() < 3)
        return "-Erros: RENAME requires old key and new key\r\n";
    if (db.rename(tokens[1], tokens[2]))
        return "+OK\r\n";
    else
        return "-Error: key not found or rename failed\r\n";   
}







RedisCommandHandler::RedisCommandHandler() {}

std::string RedisCommandHandler::processCommand(const std::string &commandLine)
{
    // Use RESP perser
    auto tokens = parseRespCommand(commandLine);
    if (tokens.empty())
        return "-Error : empty command\r\n";

    // std::cout<< commandLine << "\n";
    // for(auto& t : tokens) std::cout<<t<<"\n";

    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    std::ostringstream response;

    RedisDatabase &db = RedisDatabase::getInstance();

    // check commands

    // Comman Commads
    if (cmd == "PING")
        return handlePing(tokens, db);
    else if (cmd == "ECHO")
        return handleEcho(tokens,db);
    else if (cmd == "FLUSHALL")
        return handleFlushAll(tokens,db);
    // Key/Value Operations
    else if (cmd == "SET")
        return handleSet(tokens,db);
    else if (cmd == "GET")
        return handleGet(tokens,db);
    else if (cmd == "KEYS")
        return handleKeys(tokens, db);
    else if (cmd == "TYPE")
        return handleType(tokens,db);
    else if (cmd == "DEL" || cmd == "UNLINK")
        return handleDel(tokens, db, cmd);
    else if (cmd == "EXPIRE")
        return handleExpire(tokens,db);
    else if (cmd == "RENAME")
        return handleRename(tokens,db);
    // List Operations
    // Hash Operations
    else
    {
        response << "-ERROR: Unknown command\r\n";
    }
    return response.str();
}