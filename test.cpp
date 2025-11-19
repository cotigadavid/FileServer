// // example: /home/david/Desktop/FileServer/tools/read_db_example.cpp
// #include <sqlite3.h>
// #include <iostream>
// #include <vector>
// #include <string>

// int main(int argc, char** argv) {
//     if (argc < 2) { std::cerr << "Usage: " << argv[0] << " <dbfile>\n"; return 1; }
//     const char* dbpath = argv[1];
//     sqlite3* db = nullptr;
//     if (sqlite3_open(dbpath, &db) != SQLITE_OK) {
//         std::cerr << "open error: " << sqlite3_errmsg(db) << "\n";
//         sqlite3_close(db);
//         return 1;
//     }

//     // get table names
//     const char* q = "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';";
//     sqlite3_stmt* stmt = nullptr;
//     if (sqlite3_prepare_v2(db, q, -1, &stmt, nullptr) == SQLITE_OK) {
//         while (sqlite3_step(stmt) == SQLITE_ROW) {
//             const unsigned char* t = sqlite3_column_text(stmt, 0);
//             std::string table = t ? reinterpret_cast<const char*>(t) : "";
//             std::cout << "Table: " << table << "\n";

//             // print up to 10 rows from table
//             std::string sel = "SELECT * FROM " + table + " LIMIT 10;";
//             sqlite3_stmt* s2 = nullptr;
//             if (sqlite3_prepare_v2(db, sel.c_str(), -1, &s2, nullptr) == SQLITE_OK) {
//                 int cols = sqlite3_column_count(s2);
//                 // print header
//                 for (int c=0;c<cols;++c) {
//                     if (c) std::cout << " | ";
//                     std::cout << sqlite3_column_name(s2,c);
//                 }
//                 std::cout << "\n";
//                 while (sqlite3_step(s2) == SQLITE_ROW) {
//                     for (int c=0;c<cols;++c) {
//                         if (c) std::cout << " | ";
//                         const unsigned char* val = sqlite3_column_text(s2, c);
//                         std::cout << (val ? reinterpret_cast<const char*>(val) : "NULL");
//                     }
//                     std::cout << "\n";
//                 }
//             }
//             sqlite3_finalize(s2);
//             std::cout << "----\n";
//         }
//     }
//     sqlite3_finalize(stmt);
//     sqlite3_close(db);
//     return 0;
// }