#ifndef _SQLITE_H
#define _SQLITE_H

#include <sqlite3.h>
#include "database.h"

database *database_sqlite_open(const char *);
void database_sqlite_close(database *);
int database_sqlite_find_or_create_group(database *, const char *);
int database_sqlite_last_article_id_for_group(database *, int);
int database_sqlite_begin(database *);
int database_sqlite_commit(database *);
int database_sqlite_insert_article(database *, article *);
int database_sqlite_group_set_last_article_id(database *, int, int);

#endif
