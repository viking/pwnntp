#ifndef _ARTICLE_H
#define _ARTICLE_H

typedef struct {
  long long article_id;
  long long group_id;
  char *subject;
  int slen;
  char *message_id;
  int mlen;
  char *poster;
  int plen;
  char *posted_at;
  int wlen;
  long long bytes;
} article;

#endif
