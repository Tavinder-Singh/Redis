#ifndef REDIS_DATABASE_H
#define REDIS_DATABASE_H

#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>

class RedisDatabase{
public:
    // Get the singleton instance
    static RedisDatabase& getInstance();

    // Comman Commands
    bool flushAll();

    // Key/Value Operations
    bool set(const std::string& key, const std::string& value);
    bool get(const std::string& key, const std::string& value);
    std::vector<std::string> keys();
    std::string type(const std::string& key);
    bool del(const std::string& key);
    bool expire(const std::string& key, const std::string& seconds);
    bool rename(const std::string& oldKey, const std::string& newKey);


    // Persistance: Dump / load the database from file
    bool dump(const std::string& fileName);
    bool load(const std::string& fileName);



private:
    RedisDatabase() = default;
    ~RedisDatabase() = default;
    RedisDatabase(const RedisDatabase&) = delete;
    RedisDatabase& operator = (const RedisDatabase&) = delete;

    std::mutex db_mutex;
    std::unordered_map<std::string, std::string> kv_store;
    std::unordered_map<std::string, std::vector<std::string>> list_store;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> hash_store;

    

};

#endif