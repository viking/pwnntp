#ifndef _DATABASE_H
#define _DATABASE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "article.h"

enum stmt_types {
  blank_stmt,
  tmp_stmt,
  insert_article_stmt
};

enum db_types {
  sqlite
};

typedef struct {
  void *s_db;
  void *s_stmt;
  enum db_types db_type;
  enum stmt_types stmt_type;
} database;

database *database_open(enum db_types, ...);
void database_close(database *);
long long database_find_or_create_group(database *, const char *);
long long database_last_article_id_for_group(database *, long long);
int database_begin(database *);
int database_commit(database *);
long long database_insert_article(database *, article *);
int database_group_set_last_article_id(database *, long long, long long);

#endif
