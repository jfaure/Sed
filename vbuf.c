#include "data.h"

struct vbuf		*vbuf_new()
{
  struct vbuf 	*text;

  text = xmalloc(sizeof(*text));
  text->buf = xmalloc(text->alloc = 256);
  text->len = 0;
  return (text);
}

extern inline void		vbuf_free(struct vbuf *t)
{
  free(t->buf);
  free(t);
}

static void vbuf_testSpace(struct vbuf *text)
{
  if (text->len == text->alloc)
    text->buf = xrealloc(text->buf, text->alloc <<= 1);
}

void		vbuf_addChar(struct vbuf *text, char c)
{
  vbuf_testSpace(text);
  text->buf[text->len++] = c;
}

void		vbuf_addNull(struct vbuf *text)
{
  vbuf_testSpace(text);
  text->buf[text->len] = 0;
}

struct			vbuf	*read_text()
{
  char			c;
  struct vbuf	*text;

  text = vbuf_new(); 
  while ((c = nextChar()) != '\n')
    if (c == 0 || c == EOF)
      return (text);
    else
      vbuf_addChar(text, c);
  vbuf_addChar(text, 0);
  return (text);
}

void			vbuf_addString(struct vbuf *text, char *s, int len)
{
  if (text->len + len > text->alloc)
  {
    while (text->len > (text->alloc <<= 1));
    text->buf = realloc(text->buf, text->alloc);
  }
  memcpy(text->buf + text->len, s, len);
  text->len += len;
}

ssize_t			vbuf_getline(struct vbuf *text, FILE *in)
{
  text->len = getline(&text->buf, &text->alloc, in);
  if (text->len > 0)
    text->buf[--text->len] = 0;
  return (text->len);
}

struct vbuf		*snarf(char delim, char regex)
{ // regex can be -1 (basic), 1 (extended) or 0 (never ignore delim)
  char			in;
  struct vbuf		*text;
  int			open_paren = 0;

  text = vbuf_new();
  while ((in = nextChar()) != delim || open_paren)
  {
    if (in == EOF)
    {
      vbuf_free(text);
      return (NULL);
    }
    else if (in == '\\')
    {
      if ((in = nextChar()) == 'n')
	vbuf_addChar(text, '\n');
      else
      {
	vbuf_addChar(text, '\\');
	vbuf_addChar(text, in);
      }
      if (regex == -1)
	if (in == '(')
	  ++open_paren;
	else if (in == ')')
	  --open_paren;
      continue;
    }
    (in == '(' && regex == 1 || in == '[') && ++open_paren;
    (in == ')' && regex == 1 || in == ']') &&  --open_paren;
    vbuf_addChar(text, in);
  }
  vbuf_addChar(text, 0);
  return (text);
}

struct vbuf		*vbuf_readName()
{
  char			in;
  struct vbuf		*text;

  text = vbuf_new();
  if ((in = nextChar()) == '\\' && (in = nextChar()) == '\n')
    in = nextChar();
  while (isblank(in = nextChar()))
    ;
  while (in && in != '\n' && in != EOF)
  {
    vbuf_addChar(text, in);
    in = nextChar();
  }
  vbuf_addChar(text, 0);
  if (text->len == 0)
  {
    vbuf_free(text);
    return (NULL);
  }
  vbuf_addChar(text, 0);
  return (text);
}

char		*vbuf_tostring(struct vbuf *vbuf)
{
  char		*r;

  r = vbuf->buf;
  free(vbuf);
  return (r);
}
