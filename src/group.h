#ifndef _GROUP_H
#define _GROUP_H

#include <string.h>
#include <stdlib.h>

typedef struct {
  char *name;
  long long high;
  long long low;
  long long count;
} nntp_group;

nntp_group *nntp_group_new(const char *);
void nntp_group_free(nntp_group *);

#endif
