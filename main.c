#include "main.h"
#include "conn.h"
#include "group.h"
#include "response.h"

char *
nntp_decode_headers(data)
  const char *data;
{
  int ret, len, r_len, r_total;
  unsigned have;
  z_stream strm;
  unsigned char in[CHUNK];
  unsigned char out[CHUNK];
  const char *tail = data;
  char *r_head, *r_tail;

  /* verify yenc info */
  len = strlen(YENC_LINE);
  if (strncmp(data, YENC_LINE, len) != 0) {
    fprintf(stderr, "Bad header format.\n");
    return NULL;
  }
  tail += len;

  /* allocate inflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  ret = inflateInit2(&strm, -15);
  if (ret != Z_OK) {
    fprintf(stderr, "inflateInit failed.\n");
    return NULL;
  }

  r_head = r_tail = (char *)malloc(sizeof(char) * CHUNK);
  if (r_head == NULL) {
    (void)inflateEnd(&strm);
    fprintf(stderr, "Couldn't allocate result data.\n");
    return NULL;
  }
  r_total = CHUNK;
  r_len = 0;

  /* decompress this ish */
  do {
    if (tail == 0 || *tail == 0) {
      /* premature end */
      (void)inflateEnd(&strm);
      free(r_head);
      fprintf(stderr, "Premature end.\n");
      return NULL;
    }

    /* fill up the buffer */
    len = 0;
    while (len < CHUNK && tail) {
      if (strncmp("\r\n", tail, 2) == 0) {
        /* quit if =yend found */
        tail += 2;
        if (strncmp("=yend", tail, 5) == 0) {
          tail = 0;
          break;
        }
      }
      else {
        if (*tail != '=') {
          in[len++] = *tail - 42;
        }
        else {
          switch(*++tail) {
            default:
              fprintf(stderr, "Bad escape: \\%o\n", *tail);
            /*   NUL       TAB        LF        CR */
            case '@': case 'I': case 'J': case 'M':
            /*    =         .        ??? */
            case '}': case 'n': case '`':
              in[len++] = *tail - '@' - 42;
              break;
          }
        }
        tail++;
      }
    }

    /* inflate! */
    strm.avail_in = len;
    strm.next_in = in;
    do {
      strm.avail_out = CHUNK;
      strm.next_out = out;
      ret = inflate(&strm, Z_NO_FLUSH);
      assert(ret != Z_STREAM_ERROR);
      switch (ret) {
        case Z_NEED_DICT:
          ret = Z_DATA_ERROR;     /* and fall through */
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
          (void)inflateEnd(&strm);
          free(r_head);
          fprintf(stderr, "Error: %d\n", ret);
          return NULL;
      }

      have = CHUNK - strm.avail_out;
      if (have + r_len >= r_total) {
        r_total += CHUNK;
        r_head = (char *)realloc((void *)r_head, r_total);
        if (r_head == NULL) {
          (void)inflateEnd(&strm);
          fprintf(stderr, "Couldn't reallocate result data.\n");
          return NULL;
        }
        r_tail = r_head + r_len;
      }
      strncpy(r_tail, out, have);
      r_len += have;
      r_tail += have;
    } while (strm.avail_out == 0);

    /* done when inflate() says it's done */
  } while (ret != Z_STREAM_END);

  /* clean up and return */
  (void)inflateEnd(&strm);
  if (ret == Z_STREAM_END) {
    r_tail = 0;
    return r_head;
  }

  free(r_head);
  fprintf(stderr, "Data error.\n");
  return NULL;
}

void
nntp_init()
{
  SSL_library_init();
  SSL_load_error_strings();
  ERR_load_BIO_strings();
  OpenSSL_add_all_algorithms();
}

void
nntp_shutdown(n_conn, n_res)
  nntp_conn *n_conn;
  nntp_response *n_res;
{
  if (n_res != NULL)
    nntp_response_free(n_res);

  if (n_conn != NULL)
    nntp_conn_free(n_conn);
}

int
main(argc, argv)
  int argc;
  char *argv[];
{
  int i, article_id, res, migrate, abort, count;
  char cmd[1024], *headers, *h_cur, *h_tail, *server,
       *username, *password, *group;
  FILE *f = NULL;
  nntp_conn *n_conn = NULL;
  nntp_response *n_res = NULL;
  nntp_group *n_group = NULL;
  sqlite3 *s_db = NULL;
  sqlite3_stmt *s_stmt = NULL;

  if (argc != 5) {
    printf("Syntax: %s <server> <username> <password> <group>\n", argv[0]);
    return 1;
  }
  server = argv[1];
  username = argv[2];
  password = argv[3];
  group = argv[4];

  nntp_init();
  if ((n_conn = nntp_conn_new(server)) == NULL) {
    return 1;
  }
  if ((n_res = nntp_receive(n_conn)) == NULL) {
    nntp_conn_free(n_conn);
    return 1;
  }
  if (n_res->status != NNTP_OK) {
    fprintf(stderr, "Status wasn't OK.\n");
    nntp_shutdown(n_conn, n_res);
    return 1;
  }
  nntp_response_free(n_res); n_res = NULL;

  /* authentication */
  sprintf(cmd, "AUTHINFO USER %s\r\n", username);
  nntp_send(n_conn, cmd);
  n_res = nntp_receive(n_conn);
  if (n_res->status == NNTP_PASS_REQUIRED) {
    sprintf(cmd, "AUTHINFO PASS %s\r\n", password);
    nntp_send(n_conn, cmd);
  }
  nntp_response_free(n_res); n_res = NULL;
  n_res = nntp_receive(n_conn);
  if (n_res->status != NNTP_AUTH_OK) {
    fprintf(stderr, "Authentication was unsuccessful.\n");
    nntp_shutdown(n_conn, n_res);
    return 1;
  }
  nntp_response_free(n_res); n_res = NULL;

  /* group selection */
  sprintf(cmd, "GROUP %s\r\n", group);
  nntp_send(n_conn, cmd);
  n_res = nntp_receive(n_conn);
  if (n_res->status == NNTP_GROUP_OK) {
    n_group = (nntp_group *)n_res->data;
  }
  else {
    nntp_shutdown(n_conn, n_res);
    fprintf(stderr, "Group command wasn't successful.\n");
    return 1;
  }
  nntp_response_free(n_res); n_res = NULL;

  /* open the sqlite database */
  if ((f = fopen(DATABASE, "r")) != NULL) {
    migrate = 0;
    fclose(f);
  }
  else {
    /* create schema the first time */
    migrate = 1;
  }
  res = sqlite3_open("headers.sqlite3", &s_db);
  if (res != SQLITE_OK) {
    nntp_group_free(n_group);
    nntp_shutdown(n_conn, n_res);
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(s_db));
    return 1;
  }

  /* create schema, or find last article id */
  if (migrate) {
    article_id = 0;
    sqlite3_exec(s_db, "CREATE TABLE headers (article_id INTEGER PRIMARY KEY, subject TEXT)", NULL, NULL, NULL);
  }
  else {
    res = sqlite3_prepare_v2(s_db, "SELECT article_id FROM headers ORDER BY article_id DESC LIMIT 1", -1, &s_stmt, NULL);
    if (res != SQLITE_OK) {
      sqlite3_close(s_db);
      nntp_group_free(n_group);
      nntp_shutdown(n_conn, n_res);
      fprintf(stderr, "Couldn't prepare statement.\n");
      return 1;
    }
    res = sqlite3_step(s_stmt);
    article_id = res == SQLITE_ROW ? sqlite3_column_int(s_stmt, 0) : 0;
    sqlite3_finalize(s_stmt);
  }

  /* grab the headers! */
  res = sqlite3_prepare_v2(s_db, "INSERT INTO headers (article_id, subject) VALUES (?, ?)", -1, &s_stmt, NULL);
  if (res != SQLITE_OK) {
    free(headers);
    sqlite3_close(s_db);
    nntp_group_free(n_group);
    nntp_shutdown(n_conn, n_res);
    fprintf(stderr, "Couldn't prepare SQL statement.\n");
  }

  abort = 0;
  i = article_id == 0 ? n_group->low : article_id + 1;
  for(; i < n_group->high && abort == 0; i += LIMIT + 1) {
    count = 0;
    sprintf(cmd, "XZHDR Subject %d-%d\r\n", i, i + LIMIT);
    nntp_send(n_conn, cmd);
    n_res = nntp_receive(n_conn);
    if (n_res->status == NNTP_XZHDR_OK) {
      headers = nntp_decode_headers((char *)n_res->data);
    }
    else {
      headers = NULL;
    }
    free(n_res->data);

    if (headers == NULL) {
      sqlite3_close(s_db);
      nntp_group_free(n_group);
      nntp_shutdown(n_conn, n_res);
      fprintf(stderr, "Couldn't fetch headers.\n");
      return 1;
    }

    /* insert headers into database */
    h_tail = h_cur = headers;
    do {
      res = sqlite3_exec(s_db, "BEGIN", NULL, NULL, NULL);
      if (res == SQLITE_BUSY) {
        fprintf(stderr, "Database is busy.  Sleeping...\n");
        sleep(1);
      }
      else if (res != 0) {
        abort = 1;
        fprintf(stderr, "Couldn't start the transaction: %d\n", res);
      }
    } while (res == SQLITE_BUSY);

    while (*h_cur != 0 && abort == 0) {
      article_id = (int)strtol(h_cur, &h_cur, 10);
      while (*h_cur == ' ')
        h_cur++;

      h_tail = strstr(h_cur, "\r\n");
      if (h_tail == NULL) {
        abort = 1;
        fprintf(stderr, "Invalid header record found.\n");
        break;
      }

      sqlite3_bind_int(s_stmt, 1, article_id);
      sqlite3_bind_text(s_stmt, 2, h_cur, h_tail - h_cur, SQLITE_STATIC);
      do {
        res = sqlite3_step(s_stmt);
        if (res == SQLITE_DONE) {
          sqlite3_reset(s_stmt);
          sqlite3_clear_bindings(s_stmt);
          h_cur = h_tail + 2;
          count++;
        }
        else if (res == SQLITE_BUSY) {
          fprintf(stderr, "Database is busy.  Sleeping...\n");
          sleep(1);
        }
        else {
          abort = 1;
          fprintf(stderr, "Couldn't insert row for %d: %d\n", article_id, res);
        }
      } while (res == SQLITE_BUSY);
    }
    free(headers);

    do {
      res = sqlite3_exec(s_db, "COMMIT", NULL, NULL, NULL);
      if (res == SQLITE_BUSY) {
        fprintf(stderr, "Database is busy.  Sleeping...\n");
        sleep(1);
      }
      else if (res != 0) {
        abort = 1;
        fprintf(stderr, "Couldn't commit the transaction: %d\n", res);
      }
    } while (res == SQLITE_BUSY);
    printf("Number of valid headers for this batch: %d.\n", count);
  }

  sqlite3_finalize(s_stmt);
  sqlite3_close(s_db);
  nntp_group_free(n_group);
  nntp_shutdown(n_conn, n_res);
  return 0;
}
