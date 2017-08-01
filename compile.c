#include <stdio.h>
#include "data.h"

/*
** Prepare next file/string for nextChar()
** should allow for stuff like sed -e i -f <(echo insertthis)
*/
char	nextProgStream()
{ DBcompile("nextProgStream\n");
  if (!g_in.next)
    return EOF;
  g_in.cursor && (g_in.last = g_in.cursor[-1]);
  if (g_in.next && g_in.next->filename) // then try to read another line
  {
    g_in.next->file || (g_in.next->file = xfopen(g_in.next->filename, "r"));
    g_in.next->alloc = 0;
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
  DBcompile("adding script");
  if (!last)
    last = g_in.next = xmalloc(sizeof(*last));
  else
    last = last->next = xmalloc(sizeof(*last));
  last->next = 0;
  last->buf = str;
  last->filename = filename;
  last->file = 0;
  last->alloc = 0;
  g_in.cursor = g_in.next->buf;
}

void	bad_prog(char const *why) { fprintf(stderr, why); exit(1); } 
void	bad_cmd(char cmd) { fprintf(stderr, "Unknown Command: %c", cmd); exit(1); }

int	nextNumber(char c)
{
  int	r;

  r = 0; 
  while (isdigit(c))
    r = r * 10 + c - '0', c = nextChar();
  prevChar(c);
  return (r);
}


char			compile_address_regex(regex_t *regex, char delim, int cflags)
{
  char			in;
  struct vbuf	*text;

  delim == '\\' && (delim = nextChar());
  text = snarf(delim);
  if (!text)
    bad_cmd('/');
  regcomp(regex, text->buf, cflags);
  vbuf_free(text);
  return (0);
}

bool			compile_address(struct sedAddr *addr, char in)
{
  DBcompile("compile address, in = %c\n", in);
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
  while ((in = nextChar()) == ';' || isblank(in) || in == '\n');
  if (in == EOF) return (0);
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
  s->g = s->p = s->e = s->d = s->number = cflags = 0; s->w = NULL;
  while ((delim = nextChar()) && delim != '\n' && delim != ';' && delim != EOF)
    switch (delim)
    {
      case 'g': s->g = 1; break;
      case 'p': s->p = 1; !s->d && (s->d = !s->e); break;
      case 'e': s->e = 1; break;
      case 'm': cflags |= REG_NEWLINE; break;
      case 'i': cflags |= REG_ICASE; break;
      default: printf("S (%d):", delim); fflush(stdout); panic("unknown s optino %c\n", delim);
    }
  return (cflags);
}

char			**S_backrefs(struct SCmd *s, struct SReplacement *new, char *replace)
{
  int			i, len;

  new->text = replace;
  new->recipe = xmalloc(sizeof(*new->recipe) * (len = 10));
  i = 0;
  new->recipe[i++] = replace;
  for (; *replace; ++replace)
    if (*replace == '&')
      new->recipe[i++] = 0, *replace = 0;
    else if (*replace == '\\')
    {
      *replace = 0;
      if (*++replace >= '0' && *replace <= '9')
	new->recipe[i++] = (char *)(long) *replace - '0';
      else panic("s backref: expected number, got '%c'", *replace);
    }
  new->recipe[i] = (char *) -1; // mark end of recipe
  return (new->recipe);
}

struct SCmd 		*compile_s()
{
  struct SCmd		*s;
  char			delim;
  struct vbuf	*replace, *regex;
  int			cflags;

  s = xmalloc(sizeof(*s));
  delim = nextChar();
  delim == '\\' && (delim = nextChar());
  if (!(regex = snarf(delim)) || !(replace = snarf(delim)))
    bad_cmd('/');
  cflags = get_s_options(s);
  s->new.text = replace->buf;
  s->new.n_refs = replace->len;
  s->new.recipe = S_backrefs(s, &s->new, s->new.text);
  free(replace);
  regcomp(&s->pattern, regex->buf, cflags);
  vbuf_free(regex);
  return (s);
}

void			do_label(struct sedProgram *labelcmd, struct sedProgram *jmpcmd)
{
  static struct obstack obstack;
  static struct nameList { // private type
    char 			*name;
    struct nameList 		*next;
    struct sedProgram		*pos;
  } *labels = 0, *jumps = 0, *tmp;

  if (!labels && !jumps)
    obstack_init(&obstack);
  if (labelcmd) // add label
  {
    tmp = labels;
    labels = obstack_alloc(&obstack, sizeof(*labels));
    labels->next = tmp;
    labels->name = vbuf_tostring(vbuf_readName());
    labels->pos = labelcmd;
  }
  else if (jmpcmd) // add jmp
  {
    tmp = jumps;
    jumps = obstack_alloc(&obstack, sizeof(*labels));
    jumps->next = tmp;
    jumps->name = vbuf_tostring(vbuf_readName());
    jumps->pos = jmpcmd;
  }
  else // finished, connect all jumps
  {
    while (jumps)
    {
      for (tmp = labels; tmp; tmp = tmp->next)
	if (!strcmp(tmp->name, jumps->name))
	{
	  jumps->pos->cmd.info.jmp = tmp->pos; 
	  break;
	}
      jumps = jumps->next;
    }
    obstack_free(&obstack, NULL);
  }
}

struct sedProgram	*compile_program(struct sedProgram *const first)
{
  static struct obstack	obstack;
  struct sedProgram	*prog;
  struct sedCmd		*cmd;
  struct sedCmdAddr	*save_addr; // for cmd groups '{' '}'

  obstack_init(&obstack);
  prog = first->next = obstack_alloc(&obstack, sizeof(*prog));
  save_addr = NULL;
  for (;;)
  {
    cmd = &prog->cmd;
    cmd->cmdChar = compile_cmd_address(cmd);
    cmd->cmdChar && DBcompile("compiling %d(%c)\n", cmd->cmdChar, cmd->cmdChar);
    switch (cmd->cmdChar) // Use switch as dispatch table
    {
      case 0:   cmd->cmdChar = '#'; cmd = 0; goto finish; // only way out
      case '#': vbuf_free(vbuf_readName()); continue;
      case EOF: bad_prog("Missing command for address");
      case '{': save_addr = cmd->addr; break;
      case '}': save_addr = NULL; break;
      case 'a': case 'i': case 'c': cmd->info.text = read_text(); break;
      case 'q': case 'Q': cmd->info.int_arg = nextNumber(nextChar()); break;
      case 'r': case 'R':
      case 'w': case 'W': cmd->info.text = vbuf_readName(); break;
      case 'd': case 'D': case 'h': case 'H': case 'g': case 'G': case 'x':
      case 'l': case 'n': case 'N': case 'p': case 'P': case '=': break;
      case ':': do_label(prog, NULL); break;
      case 'b': case 't': case 'T': do_label(NULL, prog); break;
      case 's': cmd->info.s = compile_s(); break;
      case 'y': compile_y(cmd); break;
      default: DBcompile("main loop:");bad_cmd(cmd->cmdChar);
    }
    prog = prog->next = obstack_alloc(&obstack, sizeof(*prog)); // 'continue' skips this
  }
  finish:
  do_label(NULL, NULL); // connect jmps
  return(prog->next = first);
}
