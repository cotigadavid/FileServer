#include "Database.h"

void initialize_schema(Database& db) {
    db.exec(R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash BLOB NOT NULL,
            role TEXT NOT NULL DEFAULT 'user',
            created_at INTEGER NOT NULL
        );
    )");

    db.exec(R"(
        CREATE TABLE IF NOT EXISTS sessions (
            token TEXT PRIMARY KEY,
            user_id INTEGER NOT NULL,
            expires_at INTEGER NOT NULL,
            created_at INTEGER NOT NULL,
            FOREIGN KEY(user_id) REFERENCES users(id)
        );
    )");

    db.exec(R"(
        CREATE TABLE IF NOT EXISTS acl (
            path TEXT NOT NULL,
            user_id INTEGER NOT NULL,
            can_read INTEGER NOT NULL,
            can_write INTEGER NOT NULL,
            PRIMARY KEY(path, user_id),
            FOREIGN KEY(user_id) REFERENCES users(id)
        );
    )");
}
