#include "sqlite.h"

int
database_sqlite_prepare(db, stmt_type, sql)
  database *db;
  enum stmt_types stmt_type;
  const char *sql;
{
  int res;
  if (stmt_type == tmp_stmt || db->stmt_type != stmt_type) {
    if (db->stmt_type != blank_stmt)
      sqlite3_finalize((sqlite3_stmt *)db->s_stmt);

    res = sqlite3_prepare_v2((sqlite3 *)db->s_db, sql, -1, (sqlite3_stmt **)&db->s_stmt, NULL);
    if (res != SQLITE_OK) {
      fprintf(stderr, "Couldn't prepare statement: %s\n", sqlite3_errmsg((sqlite3 *)db->s_db));
      db->stmt_type = blank_stmt;
      return 1;
    }
    db->stmt_type = stmt_type;
  }
  return 0;
}

database *
database_sqlite_open(filename)
  const char *filename;
{
  int migrate, res;
  database *db;
  FILE *f;

  db = (database *)malloc(sizeof(database));
  db->s_db = NULL;
  db->s_stmt = NULL;
  db->stmt_type = blank_stmt;
  db->db_type = sqlite;

  /* open the sqlite database */
  if ((f = fopen(filename, "r")) != NULL) {
    migrate = 0;
    fclose(f);
  }
  else if (errno == ENOENT) {
    /* schema needs to be created */
    migrate = 1;
  }
  else {
    fprintf(stderr, "Couldn't open database: %s\n", strerror(errno));
    free(db);
    return NULL;
  }

  res = sqlite3_open(filename, (sqlite3 **)&db->s_db);
  if (res != SQLITE_OK) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg((sqlite3 *)db->s_db));
    database_close(db);
    return NULL;
  }
#ifdef DEBUG
  fprintf(stderr, "Opened database: %s\n", filename);
#endif

  /* create schema if necessary */
  if (migrate) {
    res = sqlite3_exec((sqlite3 *)db->s_db, "CREATE TABLE groups (id INTEGER PRIMARY KEY, name TEXT, last_article_id INTEGER)", NULL, NULL, NULL);
    if (res == 0) {
      res = sqlite3_exec((sqlite3 *)db->s_db, "CREATE TABLE articles (id INTEGER PRIMARY KEY, article_id INTEGER, group_id INTEGER, subject TEXT, message_id TEXT, poster TEXT, posted_at TEXT, bytes INTEGER)", NULL, NULL, NULL);
    }
    if (res == 0) {
      res = sqlite3_exec((sqlite3 *)db->s_db, "CREATE INDEX articles_article_id ON articles (article_id)", NULL, NULL, NULL);
    }
    if (res != 0) {
      database_close(db);
      fprintf(stderr, "Couldn't create schema: %s\n", sqlite3_errmsg((sqlite3 *)db->s_db));
      return NULL;
    }
  }

  return db;
}

void
database_sqlite_close(db)
  database *db;
{
  if (db->stmt_type != blank_stmt)
    sqlite3_finalize((sqlite3_stmt *)db->s_stmt);
  sqlite3_close((sqlite3 *)db->s_db);
  free(db);
}

int
database_sqlite_find_or_create_group(db, group)
  database *db;
  const char *group;
{
  int res, group_id;

  /* find or create group; find last article id that we know about */
  res = database_prepare(db, tmp_stmt, "SELECT id FROM groups WHERE name = ?");
  if (res > 0) {
    return -1;
  }
  sqlite3_bind_text((sqlite3_stmt *)db->s_stmt, 1, group, strlen(group), SQLITE_STATIC);
  res = sqlite3_step((sqlite3_stmt *)db->s_stmt);
  if (res == SQLITE_ROW) {
    /* group exists; get latest article_id */
    group_id = sqlite3_column_int((sqlite3_stmt *)db->s_stmt, 0);
  }
  else {
    /* create group */
    res = database_prepare(db, tmp_stmt, "INSERT INTO groups (name) VALUES (?)");
    if (res > 0) {
      return -1;
    }
    sqlite3_bind_text((sqlite3_stmt *)db->s_stmt, 1, group, strlen(group), SQLITE_STATIC);
    res = sqlite3_step((sqlite3_stmt *)db->s_stmt);

    if (res == SQLITE_DONE) {
      group_id = (int)sqlite3_last_insert_rowid((sqlite3 *)db->s_db);
    }
    else {
      fprintf(stderr, "Couldn't create group: %s\n", sqlite3_errmsg((sqlite3 *)db->s_db));
      return -1;
    }
  }
  return group_id;
}

int
database_sqlite_last_article_id_for_group(db, group_id)
  database *db;
  int group_id;
{
  int res, article_id;

  res = database_prepare(db, tmp_stmt, "SELECT last_article_id FROM groups WHERE id = ?");
  if (res > 0) {
    return -1;
  }
  sqlite3_bind_int((sqlite3_stmt *)db->s_stmt, 1, group_id);

  res = sqlite3_step((sqlite3_stmt *)db->s_stmt);
  article_id = res == SQLITE_ROW ? sqlite3_column_int((sqlite3_stmt *)db->s_stmt, 0) : 0;

  return article_id;
}

int
database_sqlite_begin(db)
  database *db;
{
  int res;
  do {
    res = sqlite3_exec((sqlite3 *)db->s_db, "BEGIN", NULL, NULL, NULL);
    if (res == SQLITE_BUSY) {
      fprintf(stderr, "Database is busy.  Sleeping...\n");
      sleep(1);
    }
    else if (res != 0) {
      fprintf(stderr, "Couldn't start the transaction: %s\n", sqlite3_errmsg((sqlite3 *)db->s_db));
      return 1;
    }
  } while (res == SQLITE_BUSY);

  return 0;
}

int
database_sqlite_commit(db)
  database *db;
{
  int res;
  do {
    res = sqlite3_exec((sqlite3 *)db->s_db, "COMMIT", NULL, NULL, NULL);
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
database_sqlite_insert_article(db, a)
  database *db;
  article *a;
{
  int res;
  res = database_prepare(db, insert_article_stmt, "INSERT INTO articles (article_id, group_id, subject, message_id, poster, posted_at, bytes) VALUES (?, ?, ?, ?, ?, ?, ?)");
  if (res > 0) {
    return -1;
  }

  sqlite3_bind_int((sqlite3_stmt *)db->s_stmt, 1, a->article_id);
  sqlite3_bind_int((sqlite3_stmt *)db->s_stmt, 2, a->group_id);
  sqlite3_bind_text((sqlite3_stmt *)db->s_stmt, 3, a->subject, a->slen, SQLITE_STATIC);
  sqlite3_bind_text((sqlite3_stmt *)db->s_stmt, 4, a->message_id, a->mlen, SQLITE_STATIC);
  sqlite3_bind_text((sqlite3_stmt *)db->s_stmt, 5, a->poster, a->plen, SQLITE_STATIC);
  sqlite3_bind_text((sqlite3_stmt *)db->s_stmt, 6, a->posted_at, a->wlen, SQLITE_STATIC);
  sqlite3_bind_int((sqlite3_stmt *)db->s_stmt, 7, a->bytes);
  while (1) {
    res = sqlite3_step((sqlite3_stmt *)db->s_stmt);
    if (res == SQLITE_DONE) {
      sqlite3_reset((sqlite3_stmt *)db->s_stmt);
      sqlite3_clear_bindings((sqlite3_stmt *)db->s_stmt);
      return (int)sqlite3_last_insert_rowid((sqlite3 *)db->s_db);
    }
    else if (res == SQLITE_BUSY) {
      fprintf(stderr, "Database is busy.  Sleeping...\n");
      sleep(1);
    }
    else {
      fprintf(stderr, "Couldn't insert row (%s)\n  article_id: %d, group_id: %d\n", sqlite3_errmsg((sqlite3 *)db->s_db), a->article_id, a->group_id);
      return -1;
    }
  }
}

int
database_sqlite_group_set_last_article_id(db, group_id, article_id)
  database *db;
  int group_id;
  int article_id;
{
  int res;
  res = database_prepare(db, tmp_stmt, "UPDATE groups SET last_article_id = ? WHERE id = ?");
  if (res > 0) {
    return -1;
  }
  sqlite3_bind_int((sqlite3_stmt *)db->s_stmt, 1, article_id);
  sqlite3_bind_int((sqlite3_stmt *)db->s_stmt, 2, group_id);
  while (1) {
    res = sqlite3_step((sqlite3_stmt *)db->s_stmt);
    if (res == SQLITE_DONE) {
      return 0;
    }
    else if (res == SQLITE_BUSY) {
      fprintf(stderr, "Database is busy.  Sleeping...\n");
      sleep(1);
    }
    else {
      fprintf(stderr, "Couldn't update group (%s)\n", sqlite3_errmsg((sqlite3 *)db->s_db));
      return -1;
    }
  }
}
