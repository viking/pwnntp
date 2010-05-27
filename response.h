#ifndef _RESPONSE_H
#define _RESPONSE_H

#include "conn.h"

#define NNTP_OK 200
#define NNTP_GROUP_OK 211
#define NNTP_XZHDR_OK 221
#define NNTP_AUTH_OK 281
#define NNTP_PASS_REQUIRED 381

typedef struct {
  char code[4];
  int  status;
  char *msg;
  int  msglen;
  void *data;
} nntp_response;

void nntp_response_free(nntp_response *);
nntp_response *nntp_receive(nntp_conn *);

#endif
