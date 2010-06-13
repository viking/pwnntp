#ifndef _ARTICLE_H
#define _ARTICLE_H

typedef struct {
  int article_id;
  int group_id;
  char *subject;
  int slen;
  char *message_id;
  int mlen;
  char *poster;
  int plen;
  char *posted_at;
  int wlen;
  int bytes;
} article;

#endif
