#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <zlib.h>
#include <getopt.h>

#define LIMIT 10000
#define CHUNK 16384
#define DEFAULT_DATABASE "pwnntp.sqlite3"
#define YENC_LINE "=ybegin line=128 size=-1\r\n"

typedef struct {
  int article_id;
  int group_id;
  char *subject;
  int slen;
  char *message_id;
  int mlen;
} article;
