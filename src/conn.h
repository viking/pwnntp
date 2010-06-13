#ifndef _CONN_H
#define _CONN_H

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct {
  BIO *bio;
  SSL_CTX *ctx;
  SSL *ssl;
} nntp_conn;

nntp_conn *nntp_conn_new(const char *);
void nntp_conn_free(nntp_conn *);
char *nntp_read(nntp_conn*, const char*, int);
int nntp_send(nntp_conn *, const char *);

#endif
