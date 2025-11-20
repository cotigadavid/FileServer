#pragma once
#include <sqlite3.h>
#include <string>
#include <stdexcept>

class Database {
public:
    Database(const std::string& file);
    ~Database();

    void exec(const std::string& sql);

    sqlite3* get_handle() { return db; }

private:
    sqlite3* db = nullptr;
};