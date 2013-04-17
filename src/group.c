#include "group.h"

void
nntp_group_free(n_group)
  nntp_group *n_group;
{
  if (n_group->name != NULL)
    free(n_group->name);

  free(n_group);
}

nntp_group *
nntp_group_new(msg)
  const char *msg;
{
  char *tail;
  nntp_group *n_group;

  n_group = (nntp_group *)malloc(sizeof(nntp_group));
  n_group->name = NULL;

  // FIXME: add error checking, and tokenize this instead
  n_group->count = strtoll(msg, &tail, 10);
  n_group->low   = strtoll(tail, &tail, 10);
  n_group->high  = strtoll(tail, &tail, 10);
  tail++;   // remove space
  n_group->name = (char *)malloc(sizeof(char) * (strlen(tail) + 1));

  strcpy(n_group->name, tail);
  return n_group;
}
