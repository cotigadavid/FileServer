#pragma once
#include <string>
#include <optional>
#include <sqlite3.h>

#include "Database.h"

class AuthManager {
public:
    AuthManager(sqlite3* db);

    bool register_user(const std::string& username, const std::string& password);
    std::optional<std::string> login(const std::string& username, const std::string& password);
    bool validate_token(const std::string& token);
    void logout(const std::string& token);

private:
    sqlite3* db;

    std::string generate_token();
    bool verify_password(const std::string& password, const std::string& stored_hash);
    std::string hash_password(const std::string& password);
};