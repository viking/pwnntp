#include "main.h"
#include "conn.h"
#include "group.h"
#include "response.h"
#include "database.h"
#include "article.h"

char *
nntp_decode_headers(data)
  const char *data;
{
  int ret, len, r_len, r_total;
  size_t ylen;
  unsigned have;
  z_stream strm;
  unsigned char in[CHUNK];
  unsigned char out[CHUNK];
  const char *tail = data;
  char *r_head, *r_tail;

  /* verify yenc info */
  ylen = strlen(YENC_LINE);
  if (strncmp(data, YENC_LINE, ylen) != 0) {
    fprintf(stderr, "Bad header format: %s\n", data);
    return NULL;
  }
  tail += (int) ylen;

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
          fprintf(stderr, "Inflate failed: %d\n", ret);
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
      strncpy(r_tail, (const char *) out, have);
      r_len += have;
      r_tail += have;
    } while (strm.avail_out == 0);

    /* done when inflate() says it's done */
  } while (ret != Z_STREAM_END);

  /* clean up and return */
  (void)inflateEnd(&strm);
  if (ret == Z_STREAM_END) {
    *r_tail = 0;
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

  nntp_send(n_conn, "QUIT\r\n");
  n_res = nntp_receive(n_conn);
  nntp_response_free(n_res);

  if (n_conn != NULL)
    nntp_conn_free(n_conn);
}

int
process_headers(n_conn, db, articles, hdr, low, high, group_id, update)
  nntp_conn *n_conn;
  database *db;
  article *articles;
  const char *hdr;
  long long low;
  long long high;
  long long group_id;
  int update;
{
  int count = 0, len;
  long long article_id;
  char tmp[1024], *headers, *h_cur, *h_tail;
  nntp_response *n_res;

  sprintf(tmp, "XZHDR %s %lld-%lld\r\n", hdr, low, high);
  nntp_send(n_conn, tmp);
  n_res = nntp_receive(n_conn);
  if (n_res->status == NNTP_XZHDR_OK) {
    headers = nntp_decode_headers((char *)n_res->data);
  }
  else {
    headers = NULL;
  }
  free(n_res->data);
  nntp_response_free(n_res);

  if (headers == NULL) {
    fprintf(stderr, "Couldn't fetch headers.\n");
    return -1;
  }
#ifdef DEBUG
  /*
  strncpy(tmp, headers, 128);
  tmp[128] = 0;
  fprintf(stderr, "First bit of headers: %s\n", tmp);
  */
#endif

  /* insert headers into database */
  h_tail = h_cur = headers;
  while (*h_cur != 0) {
    h_tail = strstr(h_cur, "\r\n");
    if (h_tail == NULL) {
      fprintf(stderr, "Invalid header record found.\n");
      break;
    }

    article_id = strtoll(h_cur, &h_cur, 10);
    if (article_id == 0) {
      fprintf(stderr, "Invalid article id.\n");
      break;
    }

    while (*h_cur == ' ')
      h_cur++;

    if (update == 0) {
      articles[count].article_id = article_id;
      articles[count].group_id = group_id;
    }
    else if (articles[count].article_id != article_id) {
      fprintf(stderr, "Article doesn't match.\n");
      break;
    }

    len = h_tail - h_cur;
    if (strcmp(hdr, "Subject") == 0) {
      articles[count].subject = (char *)malloc(sizeof(char) * len);
      strncpy(articles[count].subject, h_cur, len);
      articles[count].slen = len;
    }
    else if (strcmp(hdr, "Message-ID") == 0) {
      articles[count].message_id = (char *)malloc(sizeof(char) * len);
      strncpy(articles[count].message_id, h_cur, len);
      articles[count].mlen = len;
    }
    else if (strcmp(hdr, "From") == 0) {
      articles[count].poster = (char *)malloc(sizeof(char) * len);
      strncpy(articles[count].poster, h_cur, len);
      articles[count].plen = len;
    }
    else if (strcmp(hdr, "Date") == 0) {
      articles[count].posted_at = (char *)malloc(sizeof(char) * len);
      strncpy(articles[count].posted_at, h_cur, len);
      articles[count].wlen = len;
    }
    else if (strcmp(hdr, "Bytes") == 0) {
      articles[count].bytes = strtoll(h_cur, NULL, 10);
    }

    h_cur = h_tail + 2;
    count++;
  }
  free(headers);

#ifdef DEBUG
  fprintf(stderr, "Number of valid headers for this batch: %d.\n", count);
#endif
  return count;
}

static char timestamp[100];

void
set_timestamp()
{
  time_t t;
  struct tm *tmp;

  t = time(NULL);
  if ((tmp = localtime(&t)) == NULL) {
    fprintf(stderr, "Couldn't get localtime.\n");
    return;
  }
  if (strftime(timestamp, 100, "%a, %d %b %Y %H:%M:%S %z", tmp) == 0) {
    fprintf(stderr, "Couldn't strftime.\n");
  }
}

void
print_syntax(name)
  const char *name;
{
  printf("%s options:\n", name);
  printf("  -s, --server SERVER\n");
  printf("  -u, --user USER\n");
  printf("  -p, --password PASSWORD\n");
  printf("  -g, --group GROUP\n");
  printf("  -d, --database DATABASE   (default: pwnntp.sqlite3)\n");
  printf("  -l, --log FILE\n");
}

int
main(argc, argv)
  int argc;
  char *argv[];
{
  int j, c, count = 0, res = 0;
  long long i, article_id, group_id, group_low, group_high, upper, lower;
  char cmd[1024], *hdr;
  FILE *log = NULL;
  nntp_conn *n_conn = NULL;
  nntp_response *n_res = NULL;
  nntp_group *n_group = NULL;
  database *db = NULL;
  article articles[LIMIT];

  /* parse options */
  char *server = NULL, *user = NULL, *password = NULL, *group = NULL,
       *db_filename = DEFAULT_DATABASE, *logfile = NULL;

  while (1)
  {
    static struct option long_options[] =
    {
      {"server"  , required_argument, 0, 's'},
      {"user"    , required_argument, 0, 'u'},
      {"password", required_argument, 0, 'p'},
      {"group"   , required_argument, 0, 'g'},
      {"database", required_argument, 0, 'd'},
      {"log",      required_argument, 0, 'l'},
      {0, 0, 0, 0}
    };
    /* getopt_long stores the option index here. */
    int option_index = 0;
    c = getopt_long (argc, argv, "s:u:p:g:d:l:", long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c)
    {
      case 0:
        print_syntax(argv[0]);
        return(1);
        break;
      case 's':
        server = optarg;
        break;
      case 'u':
        user = optarg;
        break;
      case 'p':
        password = optarg;
        break;
      case 'g':
        group = optarg;
        break;
      case 'd':
        db_filename = optarg;
        break;
      case 'l':
        logfile = optarg;
        break;
      case '?':
        /* getopt_long already printed an error message. */
        break;
      default:
        print_syntax(argv[0]);
        return(1);
    }
  }
  if (server == NULL || user == NULL || password == NULL || group == NULL) {
    print_syntax(argv[0]);
    return 1;
  }
  if (logfile != NULL) {
    log = fopen(logfile, "a");
    if (log == NULL) {
      fprintf(stderr, "Couldn't open logfile %s.\n", logfile);
      return 1;
    }
    set_timestamp();
    fprintf(log, "%s: Started pwnntp\n", timestamp);
    fprintf(log, "%s:   Server: %s, User: %s, Group: %s\n", timestamp, server, user, group);
    fflush(log);
  }

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
    if (log != NULL)
      fclose(log);
    nntp_shutdown(n_conn, n_res);
    return 1;
  }
  nntp_response_free(n_res); n_res = NULL;

  /* authentication */
  sprintf(cmd, "AUTHINFO USER %s\r\n", user);
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
    if (log != NULL)
      fclose(log);
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
    group_low = n_group->low;
    group_high = n_group->high;
    nntp_group_free(n_group);
  }
  else {
    if (log != NULL)
      fclose(log);
    nntp_shutdown(n_conn, n_res);
    fprintf(stderr, "Group command wasn't successful.\n");
    return 1;
  }
  nntp_response_free(n_res); n_res = NULL;

  /* database setup */
  db = database_open(sqlite, db_filename);
  if (!db) {
    if (log != NULL)
      fclose(log);
    nntp_shutdown(n_conn, n_res);
    return 1;
  }
  group_id = database_find_or_create_group(db, group);
  if (group_id < 0) {
    if (log != NULL)
      fclose(log);
    database_close(db);
    nntp_shutdown(n_conn, n_res);
    return 1;
  }
  article_id = database_last_article_id_for_group(db, group_id);
  if (article_id < 0) {
    if (log != NULL)
      fclose(log);
    database_close(db);
    nntp_shutdown(n_conn, n_res);
    return 1;
  }
  if (article_id >= group_high) {
    if (log != NULL) {
      set_timestamp();
      fprintf(log, "%s: No articles to fetch.\n", timestamp);
      fclose(log);
    }
    database_close(db);
    nntp_shutdown(n_conn, n_res);
    return 0;
  }

  /* grab the headers! */
  i = article_id == 0 ? group_low : article_id + 1;
  while (i < group_high) {
    lower = i; upper = i + LIMIT - 1;
    if (upper > group_high)
      upper = group_high;

    if (log != NULL) {
      set_timestamp();
      fprintf(log, "%s: Headers %lld - %lld\n", timestamp, lower, upper);
      fflush(log);
    }

    for (j = 0, hdr = headers[0]; hdr != NULL; hdr = headers[++j]) {
      count = process_headers(n_conn, db, articles, hdr, lower, upper, group_id, j);
      if (count < 0) {
        fprintf(stderr, "No headers!\n");
        if (log != NULL)
          fclose(log);
        database_close(db);
        nntp_shutdown(n_conn, n_res);
        return 1;
      }
    }

    /* insert articles */
    article_id = 0;
    if (database_begin(db) > 0) {
      break;
    }
    for (j = 0; j < count; j++) {
      if (j == 0 || res > 0) {
        res = database_insert_article(db, &articles[j]);
        free(articles[j].subject);
        free(articles[j].message_id);
        free(articles[j].poster);
        free(articles[j].posted_at);
        if (res > 0) {
          article_id = articles[j].article_id;
        }
      }
    }
    if (article_id > 0) {
      database_group_set_last_article_id(db, group_id, article_id);
    }
    if (database_commit(db) > 0) {
      break;
    }

    i += LIMIT;
#ifdef DEBUG
    fprintf(stderr, "======================\n");
#endif
  }

  if (log != NULL) {
    set_timestamp();
    fprintf(log, "%s: pwnntp finished\n", timestamp);
    fclose(log);
  }
  database_close(db);
  nntp_shutdown(n_conn, n_res);
  return 0;
}
