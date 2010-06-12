#ifndef _DATABASE_H
#define _DATABASE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <errno.h>
#include "article.h"

enum stmt_types {
  blank_stmt,
  tmp_stmt,
  insert_article_stmt
};

typedef struct {
  sqlite3 *s_db;
  sqlite3_stmt *s_stmt;
  enum stmt_types stmt_type;
} database;

database *database_open(const char *);
void database_close(database *);
int database_find_or_create_group(database *, const char *);
int database_last_article_id_for_group(database *, int);
int database_begin(database *);
int database_commit(database *);
int database_insert_article(database *, article *);
int database_group_set_last_article_id(database *, int, int);

#endif
