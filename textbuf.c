#include "data.h"

struct lstring		*lstring_new()
{
  struct lstring 	*text;

  text = xmalloc(sizeof(*text));
  text->buf = xmalloc(text->alloc = 256);
  text->len = 0;
  return (text);
}

extern inline void		lstring_free(struct lstring *t)
{
  free(t->buf);
  free(t);
}

void		lstring_addChar(struct lstring *text, char c)
{
  if (text->len == text->alloc)
    text->buf = xrealloc(text->buf, text->alloc <<= 1);
  text->buf[text->len++] = c;
}

struct			lstring	*read_text()
{
  char			c;
  struct lstring	*text;

  text = lstring_new(); 
  while ((c = nextChar()) != '\n')
    if (c == 0 || c == EOF)
      return (text);
    else
      lstring_addChar(text, c);
  return (text);
}

void			lstring_addString(struct lstring *text, char *s, int len)
{
  if ((text->len += len) > text->alloc)
  {
    while (text->len > (text->alloc <<= 1));
    text->buf = realloc(text->buf, text->alloc);
  }
  memcpy(text->buf + text->len, s, len);
  text->len += len;
}

ssize_t			lstring_getline(struct lstring *text, FILE *in)
{
  return ((text->len = getline(&text->buf, &text->alloc, in)));
}

struct lstring		*snarf(char delim)
{
  char			in;
  struct lstring	*text;

  printf("snarf delim %c\n", delim);
  text = lstring_new();
  while ((in = nextChar()) != delim)
    if (in == EOF)
    {
      lstring_free(text);
      return (NULL);
    }
    else
      lstring_addChar(text, in);
  lstring_addChar(text, 0);
  return (text);
}
