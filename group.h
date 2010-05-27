#ifndef _GROUP_H
#define _GROUP_H

#include <string.h>
#include <stdlib.h>

typedef struct {
  char *name;
  long int high;
  long int low;
  long int count;
} nntp_group;

nntp_group *nntp_group_new(const char *);
void nntp_group_free(nntp_group *);

#endif
