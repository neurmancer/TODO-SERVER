#ifndef DATABASE_H
    #define DATABASE_H

#include <sqlite3.h>
#include "utils.h"


typedef void (*todo_callback)(struct todo_data *t, void *userdata);

sqlite3 *set_db(void);

//handling shit
enum STATUS add_todo(sqlite3 *db, const char *title, const char *content);
enum STATUS print_all_todo_titles(sqlite3 *db);
enum STATUS delete_todo(sqlite3 *db, int id);
enum STATUS update_todo(sqlite3 *db, int id, const char *content, int completed);
enum STATUS get_todo(sqlite3 *db, int id, struct todo_data *out);
enum STATUS foreach_todo(sqlite3 *db, todo_callback cb, void *userdata);
        
#endif