#ifndef _DATABASE_H
#define _DATABASE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

typedef struct {
  sqlite3 *s_db;
  sqlite3_stmt *s_stmt;
} database;

database *database_open(const char *);
void database_close(database *);
int database_find_or_create_group(database *, const char *);
int database_last_article_id_for_group(database *, int);

#endif
