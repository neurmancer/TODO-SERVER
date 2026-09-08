#ifndef DATABASE_H
    #define DATABASE_H

#include <sqlite3.h>
#include "utils.h"



sqlite3 *set_db(void);

//handling shit
enum STATUS print_all_todo_titles(sqlite3 *db);
enum STATUS add_todo(sqlite3 *db, struct todo_data *data);
enum STATUS delete_todo(sqlite3 *db, struct todo_data *data);
enum STATUS update_todo(sqlite3 *db, struct todo_data *data);
        
#endif