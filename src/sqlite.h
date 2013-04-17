#ifndef _SQLITE_H
#define _SQLITE_H

#include <sqlite3.h>
#include <unistd.h>
#include "database.h"

database *database_sqlite_open(const char *);
void database_sqlite_close(database *);
int database_sqlite_prepare(database *db, enum stmt_types stmt_type, const char *sql);
long long database_sqlite_find_or_create_group(database *, const char *);
long long database_sqlite_last_article_id_for_group(database *, long long);
int database_sqlite_begin(database *);
int database_sqlite_commit(database *);
long long database_sqlite_insert_article(database *, article *);
int database_sqlite_group_set_last_article_id(database *, long long, long long);

#endif
