#include "database.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "utils.h"

#define BUFFER_SIZE 512
#define TODO_RELATIVE_DB "/src/db/todo.db"

sqlite3 *set_db(void)
{
    sqlite3 *database = NULL;
    char env_buffer[BUFFER_SIZE] = {0};
    char db_path[BUFFER_SIZE] = {0};

    if (get_cwd(env_buffer, BUFFER_SIZE) == -1){ return(NULL); }

    int written = snprintf(db_path, BUFFER_SIZE, "%s%s", env_buffer, TODO_RELATIVE_DB);
    if (written < 0 || (size_t)written >= BUFFER_SIZE) {
        fprintf(stderr, "Database path too long, you absolute unit\n");
        return(NULL);
    }

    int rc = sqlite3_open(db_path, &database);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(database));
        if (database){ sqlite3_close(database); }
        return(NULL);
    }

    const char *create_sql =
        "CREATE TABLE IF NOT EXISTS TODOS ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
        "Title TEXT NOT NULL,"
        "Content TEXT NOT NULL,"
        "Completed INTEGER NOT NULL DEFAULT 0,"
        "CreatedAt INTEGER NOT NULL"
        ");";

    char *errmsg = NULL;
    rc = sqlite3_exec(database, create_sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Create table failed: %s\n", errmsg);
        sqlite3_free(errmsg);
        sqlite3_close(database);
        return(NULL);
    }

    return(database);
}

enum STATUS add_todo(sqlite3 *db, const char *title, const char *content)
{
    const char *sql = "INSERT INTO TODOS (Title, Content, CreatedAt) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "add_todo prepare: %s\n", sqlite3_errmsg(db));
        return(U_FUCKED);
    }

    time_t now = time(NULL);
    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)now);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "add_todo step: %s\n", sqlite3_errmsg(db));
        return(U_FUCKED);
    }
    return(OK);
}

enum STATUS delete_todo(sqlite3 *db, int id)
{
    const char *sql = "DELETE FROM TODOS WHERE ID = ?;";
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return(U_FUCKED);

    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return((rc == SQLITE_DONE) ? OK : U_FUCKED);
}

enum STATUS update_todo(sqlite3 *db, int id, const char *content, int completed)
{
    const char *sql = "UPDATE TODOS SET Content = ?, Completed = ? WHERE ID = ?;";
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK){
        return(U_FUCKED);
    }

    sqlite3_bind_text(stmt, 1, content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, completed);
    sqlite3_bind_int(stmt, 3, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return((rc == SQLITE_DONE) ? OK : U_FUCKED);
}

enum STATUS get_todo(sqlite3 *db, int id, struct todo_data *out)
{
    const char *sql = "SELECT ID, Title, Content, Completed, CreatedAt FROM TODOS WHERE ID = ?;";
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return(U_FUCKED);

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out->id        = sqlite3_column_int(stmt, 0);
        out->title     = strdup((const char *)sqlite3_column_text(stmt, 1));
        out->content   = strdup((const char *)sqlite3_column_text(stmt, 2));
        out->is_done   = sqlite3_column_int(stmt, 3);
        out->created_at = sqlite3_column_int64(stmt, 4);
        sqlite3_finalize(stmt);
        return(OK);
    }

    sqlite3_finalize(stmt);
    return(U_FUCKED);
}


enum STATUS foreach_todo(sqlite3 *db, todo_callback cb, void *userdata)
{
    const char *sql = "SELECT ID, Title, Content, Completed, CreatedAt FROM TODOS ORDER BY CreatedAt DESC;";
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return(U_FUCKED);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        struct todo_data t = {0};
        t.id         = sqlite3_column_int(stmt, 0);
        t.title      = (char *)sqlite3_column_text(stmt, 1);   
        t.content    = (char *)sqlite3_column_text(stmt, 2);
        t.is_done    = sqlite3_column_int(stmt, 3);
        t.created_at = sqlite3_column_int64(stmt, 4);

        cb(&t, userdata);
    }

    sqlite3_finalize(stmt);
    return(OK);
}
