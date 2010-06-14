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
#include <time.h>

#define LIMIT 10
#define CHUNK 262144
#define DEFAULT_DATABASE "pwnntp.sqlite3"
#define YENC_LINE "=ybegin line=128 size=-1\r\n"

char *headers[] = {
  "Subject", "Message-ID",
  "From", "Date", "Bytes",
  NULL
};
