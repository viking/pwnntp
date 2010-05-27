#include "response.h"
#include "group.h"

void
nntp_response_free(n_res)
  nntp_response *n_res;
{
  if (n_res->msg != NULL) {
    free(n_res->msg);
  }
  /*
  if (n_res->data != NULL) {
    switch(n_res->status) {
      case NNTP_GROUP_OK:
        nntp_group_free((nntp_group *) n_res->data);
        break;
    }
  }
  */
  free(n_res);
}

nntp_response *
nntp_receive(n_conn)
  nntp_conn *n_conn;
{
  int res, multiline = 0;
  char *tail;
  nntp_response *n_res;

  n_res = (nntp_response *)malloc(sizeof(nntp_response));
  n_res->status = 0;
  n_res->msglen = 0;
  n_res->msg = NULL;
  n_res->data = NULL;

  /* nntp response code */
  res = BIO_read(n_conn->bio, n_res->code, 3);
  if (res != 3) {
    nntp_response_free(n_res);
    fprintf(stderr, "Error: %s\n", ERR_reason_error_string(ERR_get_error()));
    return NULL;
  }
  n_res->code[3] = 0;

  /* set status code */
  if (strcmp("200", n_res->code) == 0) {
    n_res->status = NNTP_OK;
  }
  else if (strcmp("211", n_res->code) == 0) {
    n_res->status = NNTP_GROUP_OK;
  }
  else if (strcmp("221", n_res->code) == 0) {
    multiline = 1;
    n_res->status = NNTP_XZHDR_OK;
  }
  else if (strcmp("281", n_res->code) == 0) {
    n_res->status = NNTP_AUTH_OK;
  }
  else if (strcmp("381", n_res->code) == 0) {
    n_res->status = NNTP_PASS_REQUIRED;
  }
  else {
    fprintf(stderr, "Unrecognized code: <%s>\n", n_res->code);
  }

  /* nntp response body */
  n_res->msg = nntp_read(n_conn, "\r\n", 1);
#ifdef DEBUG
  fprintf(stderr, "%s: %s\n", n_res->code, n_res->msg);
#endif

  /* set data */
  if (n_res->status == NNTP_GROUP_OK) {
    n_res->data = (void *)nntp_group_new(n_res->msg);
  }
  else if (multiline) {
    /* handle multiline */
    n_res->data = (void *)nntp_read(n_conn, "\r\n.\r\n", 0);
    /*
    if (DEBUG)
      fprintf(stderr, "=====\n%s\n=====\n", (char *)n_res->data);
    */
  }

  return n_res;
}
