#include <stdio.h>
#include "data.h"

char	nextProgStream()
{ puts("nextProgStream");
  if (!g_in.next)
    return EOF;
  g_in.cursor && (g_in.last = g_in.cursor[-1]);
  if (g_in.next && g_in.next->filename) // then try to read another line
  {
    g_in.next->file || (g_in.next->file = xfopen(g_in.next->filename, "r"));
    if (getline(&g_in.next->buf, &g_in.next->alloc, g_in.next->file) != -1)
    {
      g_in.cursor = g_in.next->buf;
      return (nextChar());
    }
  fclose(g_in.next->file);
  }
  struct zbuflist *t = g_in.next; g_in.next = g_in.next->next; free(t);
  if (!g_in.next)
    return (EOF);
  puts("new script file");
  return ((g_in.cursor = g_in.next->buf) ? nextChar() : nextProgStream());
}

void			prog_addScript(char *str, char *filename)
{
  static struct zbuflist	*last = 0;
puts("adding script");
assert(str || filename);
  if (!last)
    last = g_in.next = xmalloc(sizeof(*last));
  else
    last = last->next = xmalloc(sizeof(*last));
  last->next = 0;
  last->buf = str;
  last->filename = filename;
  last->alloc = 0;
  g_in.cursor = g_in.next->buf;
}

void	bad_prog(char const *why) { fprintf(stderr, why); exit(1); } 
void	bad_cmd(char cmd) { fprintf(stderr, "Unknown Command: %c", cmd); exit(1); }

inline char	nextNonBlank()
{
  char	c;

  while ((c = getchar()) != EOF && isblank(c))
    ;
  return (c);
}

int	nextNumber(char c)
{
  int	r;

  if (!isdigit(c))
    return (0);
  r = c - '0';
  while (isdigit(c = nextChar()))
    r = r * 10 + c - '0';
  prevChar(c);
  return (r);
}

struct sedCmd		*newCmd(struct sedProgram *prog, struct sedCmdAddr *init)
{
  if (++prog->pos == prog->len)
    prog->cmdStack = xrealloc(prog->cmdStack, prog->len <<= 1);
  return (prog->cmdStack + prog->pos);
}

char			compile_address_regex(struct sedRegex *regex, char delim, int cflags)
{
  char			in;
  struct vbuf	*text;

  delim == '\\' && (delim = nextChar());
  text = snarf(delim);
  if (!text)
    bad_cmd('/');
  regcomp(&regex->compile, text->buf, regex->flags = cflags);
  vbuf_free(text);
  return (0);
}

bool			compile_address(struct sedAddr *addr, char in)
{
  printf("compile address, in = %c\n", in);
  if (isdigit(in))
  {
    addr->type = ADDR_LINE;
    addr->info.line = nextNumber(in);
  }
  else if (in == '$')
    addr->type = ADDR_END;
  else if (in == '/' || in == '\\')
  {
    addr->type = ADDR_REGEX;
    compile_address_regex(&addr->info.regex, in,
	REG_NOSUB | g_sedOptions.extended_regex_syntax * REG_EXTENDED);
  }
  else
  {
    addr->type = ADDR_NONE;
    return (false); // no address
  }
  return (true);
}
 
char			compile_cmd_address(struct sedCmd *cmd)
{ // returns cmd->cmdChar, and sets cmd->addr.
  char			in;

  cmd->addr = xmalloc(sizeof(*cmd->addr));
  cmd->addr->bang = 0;
  cmd->addr->type = CMD_ADDR_DONE;
  while ((in = nextChar()) == ';');
  if (in == EOF)
    return (0);
  if (compile_address(&cmd->addr->a1, in) == false && in != ',' && in != '~')
  {
    free(cmd->addr);
    cmd->addr = 0;
    return (in);
  }
  if (in == ',')
  {
    cmd->addr->type = CMD_ADDR_RANGE;
    compile_address(&cmd->addr->a2, nextChar());
  }
  else if (in == '~')
  {
    cmd->addr->type = CMD_ADDR_STEP;
    cmd->addr->step = nextNumber(nextChar());
  }
  else
  {
    cmd->addr->type = CMD_ADDR_LINE;
    return (nextChar());
  }
  return (nextChar());
}


static struct sedLabel	*g_first_label;

void	add_label(size_t pos, char *name)
{
  struct sedLabel	*l;

  l = xmalloc(sizeof(*l));
  l->name = name;
  l->pos = pos;
  l->next = g_first_label;
  g_first_label = l;
}

void			connect_labels(struct sedProgram *prog)
{
  struct sedLabel	*t;
  int			i;

  i = -1;
  while (++i < prog->pos)
    if (strchr("btT", prog->cmdStack[i].cmdChar))
    {
      t = g_first_label;
      while (t)
	if (strcmp(t->name, prog->cmdStack[i].info.text->buf))
	  t = t->next;
	else
	{
	  prog->cmdStack[i].info.int_arg = t->pos;
	  break ;
	}
	t || (prog->cmdStack[i].info.int_arg = 0);
    }
  while (t = g_first_label)
  {
    g_first_label = t->next;
    free(t);
  }
}

void			compile_y(struct sedCmd *cmd)
{
  unsigned char		t;
  struct vbuf	*a, *b;

  a = snarf((t = nextChar()) == '/' ? t : (t = nextChar()));
  b = snarf(t);
  if (!a || !b)
    panic("unterminated y command");
  if (a->len != b->len)
    panic("strings for 'y' command are different lengths");
  memset(cmd->info.y = xmalloc(256), 0, 256); // use trivial hash
  t = -1;
  while (++t < a->len)
    if (cmd->info.y[t] != 0)
      panic("%c is already mapped to %c", a[t], b[t]);
    else
      cmd->info.y[(int) a->buf[t]] = b->buf[t];
  vbuf_free(a); vbuf_free(b);
}

char	get_s_options(struct SCmd *s)
{
  char	cflags, delim;

  cflags = REG_EXTENDED * g_sedOptions.extended_regex_syntax;
  while ((delim = nextChar()) && delim != '\n' && delim != ';' && delim != EOF)
    switch (delim)
    {
      case 'g': s->g = 1; break;
      case 'p': s->p = 1; !s->d && (s->d = !s->e); break;
      case 'e': s->e = 1; break;
      case 'm': cflags |= REG_NEWLINE; break;
      case 'i': cflags |= REG_ICASE; break;
      default: printf("S (%d):", delim); fflush(stdout); bad_cmd(delim);
    }
  return (cflags);
}

void			S_backrefs(struct SCmd *s)
{
}

struct SCmd 		*compile_s()
{
  struct SCmd		*s;
  char			delim;
  struct vbuf	*replace, *regex;
  int			cflags;

  s = xmalloc(sizeof(*s));
  s->g = s->p = s->e = s->d = s->number = cflags = 0; s->w = NULL;
  delim = nextChar();
  delim == '\\' && (delim = nextChar());
  if (!(regex = snarf(delim)) || !(replace = snarf(delim)))
    bad_cmd('/');
  cflags = get_s_options(s);
  s->new.text = replace->buf;
  s->new.n_refs = replace->len;
  free(replace);
  regcomp(&s->pattern.compile, regex->buf, cflags);
  vbuf_free(regex);
  s->new.recipe = xmalloc(sizeof(char *) * 2);
  s->new.recipe[0] = s->new.text;
  s->new.recipe[1] = NULL;
  S_backrefs(s);
  return (s);
}

struct sedProgram	*compile_program(struct sedProgram *prog)
{
  struct sedCmd		*cmd;
  struct sedCmdAddr	*save_addr; // for cmd groups '{' '}'

  cmd = newCmd(prog, save_addr = NULL);
  while (cmd)
  {
    cmd->cmdChar = compile_cmd_address(cmd);
    cmd->cmdChar && fprintf(stderr, "compiling %d(%c)\n", cmd->cmdChar, cmd->cmdChar);
    switch (cmd->cmdChar) // Use switch as dispatch table
    { // Internally switch is a binary tree of branches (should prbly be a hash though)
      case 0:   cmd = 0; // exit while loop
      case '#': continue;
      case EOF: bad_prog("Missing command for address");
      case ':': add_label(prog->pos, snarf('\n')->buf); break;
      case '{': save_addr = cmd->addr; break;
      case '}': save_addr = NULL; break;
      case 'a': case 'i': case 'c': cmd->info.text = read_text(); break;
      case 'q': case 'Q': cmd->info.int_arg = nextNumber(nextChar()); break;
      case 'r': case 'R': // free snarf ?
      case 'w': case 'W': cmd->info.file = xfopen(snarf('\n')->buf, "rw"); break;
      case 'd': case 'D': case 'h': case 'H': case 'g': case 'G': case 'x':
      case 'l': case 'n': case 'N': case 'p': case 'P': case '=': break; // easy..
      case 'b': case 't': case 'T': cmd->info.text = snarf('\n'); break;
      case 's': cmd->info.s = compile_s(); break;
      case 'y': compile_y(cmd); break;
      default: fputs("main loop:", stdout);bad_cmd(cmd->cmdChar);
    }
    cmd = newCmd(prog, save_addr);
  }
  connect_labels(prog);
  prog->len = prog->pos;
  return (prog);
}
