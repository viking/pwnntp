#include "database.h"

database *
database_open(filename)
  const char *filename;
{
  int migrate, res;
  database *db;
  FILE *f;

  db = (database *)malloc(sizeof(database));
  db->s_db = NULL;
  db->s_stmt = NULL;

  /* open the sqlite database */
  if ((f = fopen(filename, "r")) != NULL) {
    migrate = 0;
    fclose(f);
  }
  else {
    /* schema needs to be created */
    migrate = 1;
  }

  res = sqlite3_open(filename, &db->s_db);
  if (res != SQLITE_OK) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db->s_db));
    database_close(db);
    return NULL;
  }

  /* create schema if necessary */
  if (migrate) {
    res = sqlite3_exec(db->s_db, "CREATE TABLE groups (id INTEGER PRIMARY KEY, name TEXT)", NULL, NULL, NULL);
    if (res == 0) {
      res = sqlite3_exec(db->s_db, "CREATE TABLE articles (id INTEGER PRIMARY KEY, article_id INTEGER, group_id INTEGER, subject TEXT, message_id TEXT)", NULL, NULL, NULL);
    }
    if (res == 0) {
      res = sqlite3_exec(db->s_db, "CREATE INDEX articles_article_id ON articles (article_id)", NULL, NULL, NULL);
    }
    if (res != 0) {
      database_close(db);
      fprintf(stderr, "Couldn't create schema: %s\n", sqlite3_errmsg(db->s_db));
      return NULL;
    }
  }

  return db;
}

void
database_close(db)
  database *db;
{
  sqlite3_close(db->s_db);
  free(db);
}

int
database_find_or_create_group(db, group)
  database *db;
  const char *group;
{
  int res, group_id;

  /* find or create group; find last article id that we know about */
  res = sqlite3_prepare_v2(db->s_db, "SELECT id FROM groups WHERE name = ?", -1, &db->s_stmt, NULL);
  if (res != SQLITE_OK) {
    fprintf(stderr, "Couldn't prepare statement: %s\n", sqlite3_errmsg(db->s_db));
    return -1;
  }
  sqlite3_bind_text(db->s_stmt, 1, group, strlen(group), SQLITE_STATIC);
  res = sqlite3_step(db->s_stmt);
  if (res == SQLITE_ROW) {
    /* group exists; get latest article_id */
    group_id = sqlite3_column_int(db->s_stmt, 0);
    sqlite3_finalize(db->s_stmt);
  }
  else {
    /* create group */
    sqlite3_finalize(db->s_stmt);
    res = sqlite3_prepare_v2(db->s_db, "INSERT INTO groups (name) VALUES (?)", -1, &db->s_stmt, NULL);
    if (res != SQLITE_OK) {
      fprintf(stderr, "Couldn't prepare statement: %s\n", sqlite3_errmsg(db->s_db));
      return -1;
    }
    sqlite3_bind_text(db->s_stmt, 1, group, strlen(group), SQLITE_STATIC);
    res = sqlite3_step(db->s_stmt);
    sqlite3_finalize(db->s_stmt);

    if (res == SQLITE_DONE) {
      group_id = (int)sqlite3_last_insert_rowid(db->s_db);
    }
    else {
      fprintf(stderr, "Couldn't create group: %s\n", sqlite3_errmsg(db->s_db));
      return -1;
    }
  }
  return group_id;
}

int
database_last_article_id_for_group(db, group_id)
  database *db;
  int group_id;
{
  int res, article_id;

  res = sqlite3_prepare_v2(db->s_db, "SELECT article_id FROM articles WHERE group_id = ? ORDER BY article_id DESC LIMIT 1", -1, &db->s_stmt, NULL);
  if (res != SQLITE_OK) {
    fprintf(stderr, "Couldn't prepare statement: %s\n", sqlite3_errmsg(db->s_db));
    return -1;
  }
  sqlite3_bind_int(db->s_stmt, 1, group_id);

  res = sqlite3_step(db->s_stmt);
  article_id = res == SQLITE_ROW ? sqlite3_column_int(db->s_stmt, 0) : 0;
  sqlite3_finalize(db->s_stmt);

  return article_id;
}
