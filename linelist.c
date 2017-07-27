#include "data.h"

struct lineList		*lineList_new()
{
  struct lineList	*l;

  l = xmalloc(sizeof(*l));
  l->active = l->buf = xmalloc(l->alloc = 256);
  l->len = 0;
  l->lookahead = lstring_new();
  return (l);
}

void			lineList_appendText(struct lineList *l, char *text, int len)
{
  if (l->alloc - l->len < len + 2)
    l->active = l->buf = xrealloc(l->buf, l->alloc + l->len + len);
  memcpy(l->active + l->len, text, len);
  l->len += len;
  if (!len)
  {
    l->active[++l->len] = '\n';
    l->active[++l->len] = 0;
  }
}

bool			lineList_readLine(struct lineList *l, FILE *in)
{
  ++g_lineCount;
  if (lstring_getline(l->lookahead, in) == -1)
    return (false);
  lineList_appendText(l, l->lookahead->buf, l->lookahead->len + 1);
}

void			lineList_deleteEmbeddedLine(struct lineList *l, FILE *out)
{
  char			*save;

  save = l->active;
  l->active = strchrnul(l->active, '\n');
  l->active += *l->active == '\n';
  l->len -= l->active - save;
  out && fwrite(l->active, l->active - save, 1, out);
}

void			lineList_appendLineList(struct lineList *line, struct lineList *ap)
{
  lineList_appendText(line, ap->active, ap->len);
}

void			lineList_deleteLine(struct lineList *l, FILE *out)
{
  out && fwrite(l->active, l->len, 1, out);
  l->active = l->buf;
  l->len = 0;
}
struct lineList		*lineList_new()
{
  struct lineList	*l;

  l = xmalloc(sizeof(*l));
  l->active = l->buf = xmalloc(l->alloc = 256);
  l->len = 0;
  l->lookahead = lstring_new();
  return (l);
}

void			lineList_appendText(struct lineList *l, char *text, int len)
{
  if (l->alloc - l->len < len + 2)
    l->active = l->buf = xrealloc(l->buf, l->alloc + l->len + len);
  memcpy(l->active + l->len, text, len);
  l->len += len;
  if (!len)
  {
    l->active[++l->len] = '\n';
    l->active[++l->len] = 0;
  }
}

bool			lineList_readLine(struct lineList *l, FILE *in)
{
  ++g_lineCount;
  if (lstring_getline(l->lookahead, in) == -1)
    return (false);
  lineList_appendText(l, l->lookahead->buf, l->lookahead->len + 1);
}

void			lineList_deleteEmbeddedLine(struct lineList *l, FILE *out)
{
  char			*save;

  save = l->active;
  l->active = strchrnul(l->active, '\n');
  l->active += *l->active == '\n';
  l->len -= l->active - save;
  out && fwrite(l->active, l->active - save, 1, out);
}

void			lineList_appendLineList(struct lineList *line, struct lineList *ap)
{
  lineList_appendText(line, ap->active, ap->len);
}

void			lineList_deleteLine(struct lineList *l, FILE *out)
{
  out && fwrite(l->active, l->len, 1, out);
  l->active = l->buf;
  l->len = 0;
}
