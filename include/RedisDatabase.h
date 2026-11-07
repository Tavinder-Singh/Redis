#ifndef REDIS_DATABASE_H
#define REDIS_DATABASE_H

#include <string>

class RedisDatabase{
public:
    // Get the singleton instance
    static RedisDatabase& getInstance();

    // Persistance: Dump / load the database from file
    bool dump(const std::string& fileName);
    bool load(const std::string& fileName);



private:
    RedisDatabase() = default;
    ~RedisDatabase() = default;
    RedisDatabase(const RedisDatabase&) = delete;
    RedisDatabase& operator = (const RedisDatabase&) = delete;



};

#endif