#include "database.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "utils.h"

#ifndef NULL
    #define NULL (void *)0
#endif

#define BUFFER_SIZE 512
#define TODO_RELATIVE_DB "/db/todo.db"


const char *sql_queries_or_whatever[] = {
    "INSERT INTO TODOS(Title) VALUES(?)",
    "DELETE FROM TODOS WHERE ID=?",
    "UPDATE TODOS SET COMPLETED=? WHERE ID=?"
};


enum SQL_THINGIES { 
    NEW_TODO = 0, 
    DEL_TODO = 1, 
    UPDATE_TODO = 2 
};

sqlite3 *set_db(void)
{
    //Note to future-self try goto cleanup pattern next time
    sqlite3 *database = NULL;
    char env_buffer[BUFFER_SIZE] = { 0 };
    char db_path[BUFFER_SIZE] = { 0 };

    if (get_cwd(env_buffer, BUFFER_SIZE) == -1)
    {
        return(NULL);
    }

    int written = snprintf(db_path, BUFFER_SIZE, "%s%s", env_buffer, TODO_RELATIVE_DB);

    if (written < 0 || (size_t)written >= BUFFER_SIZE) 
    {
        fprintf(stderr, "Error: Database path exceeds BUFFER_SIZE.\n");
        return(NULL);
    }
    
    int rc = sqlite3_open(db_path, &database);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open the database: %s\n", sqlite3_errmsg(database));
        if (database != NULL) {
            sqlite3_close(database);
        }
        return(NULL);
    }

    return(database);   
}

enum STATUS add_todo(sqlite3 *db, struct todo_data *data)
{
    sqlite3_stmt *res = NULL;

    int rc = sqlite3_prepare_v2(db, sql_queries_or_whatever[NEW_TODO], -1, &res, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "To-Do prepare error: %s\n", sqlite3_errmsg(db));
        return(U_FUCKED);
    }

    if(sqlite3_bind_text(res, 1, data->title, -1, SQLITE_TRANSIENT) != SQLITE_OK)
    {
        fprintf(stderr, "First bind err: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(res); 
        return(U_FUCKED);
    }

    rc = sqlite3_step(res);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Step error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(res);
        return(U_FUCKED);
    }

    sqlite3_finalize(res);
    return(OK);
}

enum STATUS delete_todo(sqlite3 *db, struct todo_data *data)
{
    sqlite3_stmt *res = NULL;

    int rc = sqlite3_prepare_v2(db, sql_queries_or_whatever[DEL_TODO], -1, &res, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Delete prepare error: %s\n", sqlite3_errmsg(db));
        return(U_FUCKED);
    }

    if (sqlite3_bind_int(res, 1, data->id) != SQLITE_OK) {
        fprintf(stderr, "Delete bind err: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(res);
        return(U_FUCKED);
    }

    rc = sqlite3_step(res);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Delete step error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(res);
        return(U_FUCKED);
    }

    sqlite3_finalize(res);
    return(OK);
}

enum STATUS update_todo(sqlite3 *db, struct todo_data *data)
{
    sqlite3_stmt *res = NULL;

    int rc = sqlite3_prepare_v2(db, sql_queries_or_whatever[UPDATE_TODO], -1, &res, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Update prepare error: %s\n", sqlite3_errmsg(db));
        return(U_FUCKED);
    }

    
    if (sqlite3_bind_int(res, 1, data->is_done) != SQLITE_OK) {
        fprintf(stderr, "Update completed bind err: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(res);
        return(U_FUCKED);
    }

    if (sqlite3_bind_int(res, 2, data->id) != SQLITE_OK) {
        fprintf(stderr, "Update ID bind err: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(res);
        return(U_FUCKED);
    }

    rc = sqlite3_step(res);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Update step error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(res);
        return(U_FUCKED);
    }

    sqlite3_finalize(res);
    return(OK);
}

//Demo CLI version before implementing this shit to web
enum STATUS print_all_todo_titles(sqlite3 *db)
{
    sqlite3_stmt *res = NULL;
    
    const char *sql = "SELECT Title FROM TODOS;";

    int rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return(U_FUCKED);
    }

    printf("--- TODO TITLES ---\n");
    
    while (sqlite3_step(res) == SQLITE_ROW) {
        const unsigned char *title = sqlite3_column_text(res, 0);
        
        if (title != NULL) {
            printf("- %s\n", title);
        }
    }

    sqlite3_finalize(res);
    return(OK);
}