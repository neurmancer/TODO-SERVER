#define BUFFER_SIZE 512
#define TODO_RELATIVE_DB "/db/todo.db"

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

char *sql_queries_or_whatever[] = {
    "insert into TODOS(Title,Completed) values(?,?)",


};

enum SQL_THINGIES { NEW_TODO , DEL_TODO, UPDATE_TODO};
enum SQL_TAGS {TITLE = 1, COMPLETED = 2};

sqlite3 *set_db(void)
{
    sqlite3 *database = NULL;
    char env_buffer[BUFFER_SIZE] = { 0 };
    char db_path[BUFFER_SIZE] = { 0 };

    if (get_env(env_buffer, BUFFER_SIZE) == -1)
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
        fprintf(stderr, "To-Do couldn't added:%s\n", sqlite3_errmsg(db));
        return(U_FUCKED);
    }

    if(sqlite3_bind_text(res, TITLE,data->title,-1, SQLITE_TRANSIENT) != SQLITE_OK)
    {
        fprintf(stderr, "first bind err: %s\n", sqlite3_errmsg(db));
        return(U_FUCKED);
    }

    if(sqlite3_bind_int(res, COMPLETED,data->is_done) != SQLITE_OK)
    {
        fprintf(stderr, "FUCKKKK Second bind err: %s\n", sqlite3_errmsg(db));
        return(U_FUCKED);
    }
    
    rc = sqlite3_step(res);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Something happeend it is:%s\n", sqlite3_errmsg(db));
        return(U_FUCKED);
    }

    sqlite3_finalize(res);
    return(OK);
}

enum STATUS delete_todo(sqlite3 *db, struct todo_data *data)
{


    return(OK);
}

enum STATUS update_todo(sqlite3 *db, struct todo_data *data)
{


    return(OK);
}

