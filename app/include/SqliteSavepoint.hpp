#pragma once

#include <sqlite3.h>

/** @brief Rolls back a cache write unless all related statements commit successfully. */
class SqliteSavepoint {
public:
    explicit SqliteSavepoint(sqlite3* connection)
        : connection_(connection),
          active_(sqlite3_exec(connection_, "SAVEPOINT aifs_cache_write;", nullptr, nullptr, nullptr) == SQLITE_OK)
    {
    }

    ~SqliteSavepoint()
    {
        if (active_) {
            sqlite3_exec(connection_, "ROLLBACK TO aifs_cache_write; RELEASE aifs_cache_write;",
                         nullptr, nullptr, nullptr);
        }
    }

    SqliteSavepoint(const SqliteSavepoint&) = delete;
    SqliteSavepoint& operator=(const SqliteSavepoint&) = delete;

    explicit operator bool() const { return active_; }

    bool commit()
    {
        if (!active_ || sqlite3_exec(connection_, "RELEASE aifs_cache_write;", nullptr, nullptr, nullptr) != SQLITE_OK) {
            return false;
        }
        active_ = false;
        return true;
    }

private:
    sqlite3* connection_;
    bool active_;
};
