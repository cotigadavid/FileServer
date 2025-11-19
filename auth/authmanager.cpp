#include "authmanager.h"
#include <sodium.h>
#include <iostream>
#include <ctime>

AuthManager::AuthManager(sqlite3* db)
    : db(db)
{
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium init failed");
    }
}

std::string AuthManager::hash_password(const std::string& password) {
    char hash[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(
            hash,
            password.c_str(),
            password.size(),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE
        ) != 0)
    {
        throw std::runtime_error("Out of memory hashing password");
    }

    return std::string(hash);
}

bool AuthManager::verify_password(const std::string& password, const std::string& stored_hash) {
    return crypto_pwhash_str_verify(
        stored_hash.c_str(),
        password.c_str(),
        password.size()
    ) == 0;
}

std::string AuthManager::generate_token() {
    unsigned char token_bin[32];
    char token_hex[65];

    randombytes_buf(token_bin, sizeof(token_bin));
    sodium_bin2hex(token_hex, sizeof(token_hex), token_bin, sizeof(token_bin));

    return std::string(token_hex);
}

bool AuthManager::register_user(const std::string& username, const std::string& password) {
    std::string pw_hash = hash_password(password);

    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO users (username, password_hash, created_at) VALUES (?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "register_user prepare failed: " << sqlite3_errmsg(db) << "\n";
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, pw_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)time(nullptr));

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        std::cerr << "register_user step failed: " << sqlite3_errmsg(db) << "\n";
    }

    sqlite3_finalize(stmt);
    return ok;
}

std::optional<std::string> AuthManager::login(const std::string& username, const std::string& password) {
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, password_hash FROM users WHERE username = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "login prepare failed: " << sqlite3_errmsg(db) << "\n";
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    int user_id = sqlite3_column_int(stmt, 0);
    const unsigned char* ph = sqlite3_column_text(stmt, 1);
    std::string stored_hash = ph ? reinterpret_cast<const char*>(ph) : "";

    sqlite3_finalize(stmt);

    if (!verify_password(password, stored_hash))
        return std::nullopt;

    // generate token and store in sessions table
    std::string token = generate_token();
    time_t now = time(nullptr);
    time_t expires = now + 24*3600; // 24h

    sqlite3_stmt* ins;
    const char* ins_sql = "INSERT INTO sessions (token, user_id, expires_at, created_at) VALUES (?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, ins_sql, -1, &ins, nullptr) != SQLITE_OK) {
        std::cerr << "login insert prepare failed: " << sqlite3_errmsg(db) << "\n";
        return std::nullopt;
    }

    sqlite3_bind_text(ins, 1, token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(ins, 2, user_id);
    sqlite3_bind_int64(ins, 3, (sqlite3_int64)expires);
    sqlite3_bind_int64(ins, 4, (sqlite3_int64)now);

    bool ok = sqlite3_step(ins) == SQLITE_DONE;
    if (!ok) {
        std::cerr << "login insert failed: " << sqlite3_errmsg(db) << "\n";
        sqlite3_finalize(ins);
        return std::nullopt;
    }

    sqlite3_finalize(ins);
    return token;
}

bool AuthManager::validate_token(const std::string& token) {
    sqlite3_stmt* stmt;
    const char* sql = "SELECT user_id FROM sessions WHERE token = ? AND expires_at > ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "validate_token prepare failed: " << sqlite3_errmsg(db) << "\n";
        return false;
    }

    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)time(nullptr));

    bool valid = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return valid;
}

void AuthManager::logout(const std::string& token) {
    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM sessions WHERE token = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "logout prepare failed: " << sqlite3_errmsg(db) << "\n";
        return;
    }

    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "logout step failed: " << sqlite3_errmsg(db) << "\n";
    }

    sqlite3_finalize(stmt);
}
