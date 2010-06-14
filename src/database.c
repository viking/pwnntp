#include "database.h"
#include "sqlite.h"

int
database_prepare(db, stmt_type, sql)
  database *db;
  enum stmt_types stmt_type;
  const char *sql;
{
  switch (db->db_type) {
    case sqlite:
      return database_sqlite_prepare(db, stmt_type, sql);
  }
}

database *
database_open(enum db_types db_type, ...)
{
  va_list ap;
  char *filename;

  va_start(ap, db_type);
  switch (db_type) {
    case sqlite:
      filename = va_arg(ap, char *);
      va_end(ap);
      return database_sqlite_open(filename);
  }
}

void
database_close(db)
  database *db;
{
  switch (db->db_type) {
    case sqlite:
      database_sqlite_close(db);
  }
}

int
database_find_or_create_group(db, group)
  database *db;
  const char *group;
{
  switch (db->db_type) {
    case sqlite:
      return database_sqlite_find_or_create_group(db, group);
  }
}

int
database_last_article_id_for_group(db, group_id)
  database *db;
  int group_id;
{
  switch (db->db_type) {
    case sqlite:
      return database_sqlite_last_article_id_for_group(db, group_id);
  }
}

int
database_begin(db)
  database *db;
{
  switch (db->db_type) {
    case sqlite:
      return database_sqlite_begin(db);
  }
}

int
database_commit(db)
  database *db;
{
  switch (db->db_type) {
    case sqlite:
      return database_sqlite_commit(db);
  }
}

int
database_insert_article(db, a)
  database *db;
  article *a;
{
  switch (db->db_type) {
    case sqlite:
      return database_sqlite_insert_article(db, a);
  }
}

int
database_group_set_last_article_id(db, group_id, article_id)
  database *db;
  int group_id;
  int article_id;
{
  switch (db->db_type) {
    case sqlite:
      return database_sqlite_group_set_last_article_id(db, group_id, article_id);
  }
}
