#include "conn.h"

void
nntp_conn_free(n_conn)
  nntp_conn *n_conn;
{
  if (n_conn->bio != NULL)
    BIO_free_all(n_conn->bio);

  if (n_conn->ctx != NULL)
    SSL_CTX_free(n_conn->ctx);

  free(n_conn);
}

nntp_conn *
nntp_conn_new(server)
  const char *server;
{
  nntp_conn *n_conn;

  n_conn = (nntp_conn *)malloc(sizeof(nntp_conn));
  n_conn->ctx = NULL;
  n_conn->bio = NULL;

  n_conn->ctx = SSL_CTX_new(SSLv23_client_method());
  if (!SSL_CTX_load_verify_locations(n_conn->ctx, NULL, "/etc/ssl/certs")) {
    nntp_conn_free(n_conn);
    printf("Error: %s\n", ERR_reason_error_string(ERR_get_error()));
    return NULL;
  }

  n_conn->bio = BIO_new_ssl_connect(n_conn->ctx);
  BIO_get_ssl(n_conn->bio, &n_conn->ssl);
  SSL_set_mode(n_conn->ssl, SSL_MODE_AUTO_RETRY);

  BIO_set_conn_hostname(n_conn->bio, server);
  if (BIO_do_connect(n_conn->bio) <= 0) {
    nntp_conn_free(n_conn);
    printf("Error: %s\n", ERR_reason_error_string(ERR_get_error()));
    return NULL;
  }

  if (SSL_get_verify_result(n_conn->ssl) != X509_V_OK) {
    nntp_conn_free(n_conn);
    printf("Error: %s\n", ERR_reason_error_string(ERR_get_error()));
    return NULL;
  }

  return(n_conn);
}

char *
nntp_read(n_conn, sentinel, strip)
  nntp_conn *n_conn;
  const char *sentinel;
  int strip;
{
  int len = 0, slen = strlen(sentinel);
  char *head, *tail;
  int res;

  head = tail = (char *)malloc(sizeof(char) * 1024);
  while (1) {
    res = BIO_read(n_conn->bio, tail, 1);
    if (res < 0) {
      free(head);
      fprintf(stderr, "Error: %s\n", ERR_reason_error_string(ERR_get_error()));
      return NULL;
    }
    if (res == 0) {
      break;
    }

    if (len > 0 && len % 1023 == 0) {
      head = (char *)realloc((void *)head, sizeof(char) * (len + 1024));
      tail = head + len;
    }

    /* check for sentinel */
    if (len >= slen && strncmp(tail-(slen-1), sentinel, slen) == 0) {
      break;
    }

    if (strip == 0 || *tail != ' ' || len > 0) {
      // strip leading space
      len++; tail++;
    }
  }
  *(tail-(slen-1)) = 0;
  return head;
}

int
nntp_send(n_conn, cmd)
  nntp_conn *n_conn;
  const char *cmd;
{
  int res, len = strlen(cmd);
#ifdef DEBUG
  fprintf(stderr, "%s", cmd);
#endif
  res = BIO_write(n_conn->bio, cmd, len);
  if (res != len) {
    fprintf(stderr, "Error: %s\n", ERR_reason_error_string(ERR_get_error()));
    return 1;
  }
  return 0;
}
