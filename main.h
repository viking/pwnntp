#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sqlite3.h>
#include <zlib.h>

#define LIMIT 10000
#define CHUNK 16384
#define DATABASE "headers.sqlite3"
#define YENC_LINE "=ybegin line=128 size=-1\r\n"
