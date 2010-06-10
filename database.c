#include "database.h"

static int
database_prepare(db, stmt_type, sql)
  database *db;
  enum stmt_types stmt_type;
  const char *sql;
{
  int res;
  if (stmt_type == tmp_stmt || db->stmt_type != stmt_type) {
    if (db->stmt_type != blank_stmt)
      sqlite3_finalize(db->s_stmt);

    res = sqlite3_prepare_v2(db->s_db, sql, -1, &db->s_stmt, NULL);
    if (res != SQLITE_OK) {
      fprintf(stderr, "Couldn't prepare statement: %s\n", sqlite3_errmsg(db->s_db));
      db->stmt_type = blank_stmt;
      return 1;
    }
    db->stmt_type = stmt_type;
  }
  return 0;
}

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
  db->stmt_type = blank_stmt;

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
  if (db->stmt_type != blank_stmt)
    sqlite3_finalize(db->s_stmt);
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
  res = database_prepare(db, tmp_stmt, "SELECT id FROM groups WHERE name = ?");
  if (res > 0) {
    return -1;
  }
  sqlite3_bind_text(db->s_stmt, 1, group, strlen(group), SQLITE_STATIC);
  res = sqlite3_step(db->s_stmt);
  if (res == SQLITE_ROW) {
    /* group exists; get latest article_id */
    group_id = sqlite3_column_int(db->s_stmt, 0);
  }
  else {
    /* create group */
    res = database_prepare(db, tmp_stmt, "INSERT INTO groups (name) VALUES (?)");
    if (res > 0) {
      return -1;
    }
    sqlite3_bind_text(db->s_stmt, 1, group, strlen(group), SQLITE_STATIC);
    res = sqlite3_step(db->s_stmt);

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

  res = database_prepare(db, tmp_stmt, "SELECT article_id FROM articles WHERE group_id = ? ORDER BY article_id DESC LIMIT 1");
  if (res > 0) {
    return -1;
  }
  sqlite3_bind_int(db->s_stmt, 1, group_id);

  res = sqlite3_step(db->s_stmt);
  article_id = res == SQLITE_ROW ? sqlite3_column_int(db->s_stmt, 0) : 0;

  return article_id;
}

int
database_begin(db)
  database *db;
{
  int res;
  do {
    res = sqlite3_exec(db->s_db, "BEGIN", NULL, NULL, NULL);
    if (res == SQLITE_BUSY) {
      fprintf(stderr, "Database is busy.  Sleeping...\n");
      sleep(1);
    }
    else if (res != 0) {
      fprintf(stderr, "Couldn't start the transaction: %s\n", sqlite3_errmsg(db->s_db));
      return 1;
    }
  } while (res == SQLITE_BUSY);

  return 0;
}

int
database_commit(db)
  database *db;
{
  int res;
  do {
    res = sqlite3_exec(db->s_db, "COMMIT", NULL, NULL, NULL);
    if (res == SQLITE_BUSY) {
      fprintf(stderr, "Database is busy.  Sleeping...\n");
      sleep(1);
    }
    else if (res != 0) {
      fprintf(stderr, "Couldn't commit the transaction: %d\n", res);
      return 1;
    }
  } while (res == SQLITE_BUSY);

  return 0;
}

int
database_insert_article(db, article_id, group_id, subject, slen, message_id, mlen)
  database *db;
  int article_id;
  int group_id;
  const char *subject;
  int slen;
  const char *message_id;
  int mlen;
{
  int res;
  res = database_prepare(db, insert_article_stmt, "INSERT INTO articles (article_id, group_id, subject, message_id) VALUES (?, ?, ?, ?)");
  if (res > 0) {
    return -1;
  }

  sqlite3_bind_int(db->s_stmt, 1, article_id);
  sqlite3_bind_int(db->s_stmt, 2, group_id);
  sqlite3_bind_text(db->s_stmt, 3, subject, slen, SQLITE_STATIC);
  sqlite3_bind_text(db->s_stmt, 4, message_id, mlen, SQLITE_STATIC);
  while (1) {
    res = sqlite3_step(db->s_stmt);
    if (res == SQLITE_DONE) {
      sqlite3_reset(db->s_stmt);
      sqlite3_clear_bindings(db->s_stmt);
      return (int)sqlite3_last_insert_rowid(db->s_db);
    }
    else if (res == SQLITE_BUSY) {
      fprintf(stderr, "Database is busy.  Sleeping...\n");
      sleep(1);
    }
    else {
      fprintf(stderr, "Couldn't insert row (%s)\n  article_id: %d, group_id: %d\n", sqlite3_errmsg(db->s_db), article_id, group_id);
      return -1;
    }
  }
}
