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
    fprintf(stderr, "Couldn't load certs: %s\n", ERR_reason_error_string(ERR_get_error()));
    return NULL;
  }

  n_conn->bio = BIO_new_ssl_connect(n_conn->ctx);
  BIO_get_ssl(n_conn->bio, &n_conn->ssl);
  SSL_set_mode(n_conn->ssl, SSL_MODE_AUTO_RETRY);

  BIO_set_conn_hostname(n_conn->bio, server);
  if (BIO_do_connect(n_conn->bio) <= 0) {
    nntp_conn_free(n_conn);
    fprintf(stderr, "Couldn't connect to host: %s\n", ERR_reason_error_string(ERR_get_error()));
    return NULL;
  }

  if (SSL_get_verify_result(n_conn->ssl) != X509_V_OK) {
    nntp_conn_free(n_conn);
    fprintf(stderr, "Couldn't verify: %s\n", ERR_reason_error_string(ERR_get_error()));
    return NULL;
  }

  return(n_conn);
}

char *
nntp_read(n_conn, sentinel, chomp)
  nntp_conn *n_conn;
  const char *sentinel;
  int chomp;
{
  size_t len = 0, slen = strlen(sentinel);
  char *head, *tail, *new_head;
  int res, sindex = 0;

  head = tail = (char *)malloc(sizeof(char) * 1024);
  if (head == NULL) {
    perror("malloc");
    return NULL;
  }
  while (1) {
    res = BIO_read(n_conn->bio, tail, 1);
    if (res < 0) {
      free(head);
      fprintf(stderr, "Couldn't read: %s\n", ERR_reason_error_string(ERR_get_error()));
      return NULL;
    }
    if (res == 0) {
      break;
    }
    len++;

    if (len % 1024 == 0) {
      /* make sure we have enough space for string + null */
      new_head = (char *)realloc((void *)head, sizeof(char) * (len + 1024));
      if (new_head == NULL) {
        free(head);
        perror("realloc");
        return NULL;
      }
      head = new_head;
      tail = head + ((int) len) - 1;
    }

    /* check for sentinel */
    if (*tail == sentinel[sindex]) {
      sindex++;
      if (sindex == ((int) slen)) {
        /* sentinel found */
        break;
      }
    }
    else {
      sindex = 0;
    }
    tail++;
  }
  *(tail-chomp+1) = 0;
  return head;
}

int
nntp_send(n_conn, cmd)
  nntp_conn *n_conn;
  const char *cmd;
{
  int res;
  size_t len = strlen(cmd);
#ifdef DEBUG
  fprintf(stderr, "%s", cmd);
#endif
  res = BIO_write(n_conn->bio, cmd, (int) len);
  if (res != ((int) len)) {
    fprintf(stderr, "Couldn't write: %s\n", ERR_reason_error_string(ERR_get_error()));
    return 1;
  }
  return 0;
}
