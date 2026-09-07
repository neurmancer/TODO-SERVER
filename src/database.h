#ifndef DATABASE_H
    #define DATABASE_H

#include <sqlite3.h>
#include "utils.h"
//Why doing this? 'cuz I don't want some prick with '; DROP TABLE 'todos'; trick fuck up my todo app with a fucking injection


//note to future-self change tha names

sqlite3 *set_db(void);

//handling shit
enum STATUS add_todo(sqlite3 *db, struct todo_data *data);
enum STATUS delete_todo(sqlite3 *db, struct todo_data *data);
enum STATUS update_todo(sqlite3 *db, struct todo_data *data);
        
#endif