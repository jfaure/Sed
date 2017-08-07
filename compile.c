#include <stdio.h>
#include "data.h"

char	nextChar()
{
  *g_in.cursor == '\n' && ++g_in.line;
  if (*g_in.cursor)
    return (*g_in.cursor++);
  else
    return (nextProgStream());
}

#define prevChar(c) *--g_in.cursor; // This should obviously never be called accross a stream boundary (ie: c == '\n')

int	nextNum(char c)
{
  int	r = 0;
  while (isdigit(c))
    r = r * 10 + c - '0', c = nextChar();
  prevChar(c);
  return (r);
}

int	nextNonBlank()
{
  char	c;
  while (isblank(c = nextChar()))
    ;
  return (c);
}

/*
** Prepare next file/string for nextChar()
** this implementation allows for stuff like sed -e i\ -f <(echo insertthis)
** Importantly, this function insert a '\n' between streams
*/
char	nextProgStream()
{
  if (!g_in.info)
    return EOF;
  g_in.cursor && (g_in.last = g_in.cursor[-1]); // !! why
  if (g_in.info && g_in.info->filename) // then try to read another line
  {
    g_in.info->file || (g_in.info->file = xfopen(g_in.info->filename, "r"));
    g_in.info->alloc = 0;
    if (getline(&g_in.info->buf, &g_in.info->alloc, g_in.info->file) != -1)
    {
      g_in.cursor = g_in.info->buf;
      return ('\n');
    }
  fclose(g_in.info->file);
  }
  struct zbuflist *t = g_in.info; g_in.info = g_in.info->next; free(t);
  if (!g_in.info)
    return (EOF);
  return ((g_in.cursor = g_in.info->buf) ? '\n' : nextProgStream());
}

void			prog_addScript(char *str, char *filename)
{
  static struct zbuflist	*last = 0;

  if (!last)
  {
    last = g_in.info = xmalloc(sizeof(*last));
    g_in.line = 1;
  }
  else
    last = last->next = xmalloc(sizeof(*last));
  last->next = last->file = last->alloc = 0;
  last->buf = str;
  last->filename = filename;
  g_in.cursor = g_in.info->buf;
}

char			compile_address_regex(regex_t *regex, char delim)
{
  char			in;
  struct vbuf		*text;
  int			cflags;

  cflags = REG_NOSUB | g_sedOptions.extended_regex_syntax * REG_EXTENDED
  if ((text = snarf(delim)) == NULL)
    panic("unterminated address regex (expected '%c')", delim);
  while ((in = nextChar()) == 'I' || in == 'M')
    cflags |= in == 'I' ? REG_ICASE : REG_NEWLINE;
  regcomp(regex, text->buf, cflags);
  vbuf_free(text);
  return (in);
}

char			compile_address(struct sedAddr *addr, char in)
{
  if (in == '-' || in == '$' || isdigit(in))
  {
    addr->type = ADDR_LINE;
    if (in == '-')
      addr->info.line = -nextNum(nextChar()), in = nextChar();
    else if (in == '$')
      addr->info.line = -1, in = nextChar();
    else
      addr->info.line = nextNum(&in);
    if (addr->info.line < g_lineInfo.lookahead)
      g_lineInfo.lookahead = addr->info.line;
  }
  else if (in == '/' || in == '\\') {
    addr->type = ADDR_REGEX;
    in == '\\' && (in = nextChar());
    in = compile_address_regex(&addr->info.regex, in);
  }
  else
    addr->type = ADDR_NONE;
  return (in);
}

// sets cmd address and returns the command char 
char			compile_cmd_address(struct sedCmd *cmd)
{
  char			in;

  cmd->addr = xmalloc(sizeof(*cmd->addr));
  cmd->addr->bang = 0;
  cmd->addr->type = CMD_ADDR_DONE;
  while ((in = nextChar()) == ';' || isblank(in) || in == '\n');
  if (in == EOF) return (0);
  in = compile_address(&cmd->addr->a1, in);
  if (cmd->addr->a1.type == ADDR_NONE && in != ',' && in != '~')
  {
    free(cmd->addr);
    cmd->addr = NULL;
    return (in);
  }
  if (in == ',')
  {
    cmd->addr->type = CMD_ADDR_RANGE;
    in = compile_address(&cmd->addr->a2, nextChar());
  }
  else if (in == '~')
  {
    cmd->addr->type = CMD_ADDR_STEP;
    in = nextChar();
    cmd->addr->step = nextNum(in);
    return (in);
  }
  else
    cmd->addr->type = CMD_ADDR_LINE;
  if (in == '!')
    cmd->addr->bang = 1, in = nextChar();
  return (in);
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
  memset(cmd->info.y = xmalloc(256), 0, 256); // trivial hash
  for (t = 0; ++t; t < a->len)
    if (cmd->info.y[t] != 0)
      panic("y: %c is already mapped to %c", a[t], b[t]);
    else
      cmd->info.y[(int) a->buf[t]] = b->buf[t];
  vbuf_free(a); vbuf_free(b);
}

char	get_s_options(struct SCmd *s)
{
  char	cflags, delim;

  cflags = REG_EXTENDED * g_sedOptions.extended_regex_syntax;
  s->g = s->p = s->e = s->d = s->number = cflags = 0; s->w = NULL;
  while (delim = nextChar())
    switch (delim) { // missing some extensions
    case 'g': s->g = 1; break;
    case 'p': s->p = 1; !s->d && (s->d = !s->e); break;
    case 'e': s->e = 1; break;
    case 'm': case 'M': cflags |= REG_NEWLINE; break;
    case 'i': case 'I': cflags |= REG_ICASE; break;
    case '}': case '#': prevChar(delim);
    case ';': case '\r': case '\n': case EOF: return (cflags);
    default: panic("s: unknown option '%c'", delim);
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
    panic("unterminated delimiter %c", '/');
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
  } 				*labels = 0, *jumps = 0, *tmp;

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
    for ( ; jumps; jumps = jumps->next)
      for (tmp = labels; tmp; tmp = tmp->next)
	if (!strcmp(tmp->name, jumps->name))
	{
	  jumps->pos->cmd.info.jmp = tmp->pos; 
	  break;
	}
    obstack_free(&obstack, NULL);
  }
}

#define compile_lbrace if (l_paren) panic("Too many '{'");\
                     else l_paren = &prog->cmd.info.jmp, \
		       prog->cmd.addr->bang = !prog->cmd.addr->bang;

#define compile_rbrace if (!l_paren) panic("unexpected '}'");\
  		     else *l_paren = prog; l_paren = NULL;

void	check_trailing_chars(char cmd)
{
  if (!strchr(";\n}", cmd = nextNonBlank()) && cmd != EOF)
    panic("extra char after command: '%c'(%d)", cmd, cmd);
}

// add 'e' and 'F' commands ?
struct sedProgram	*compile_program(struct sedProgram *const first)
{
  static struct obstack	obstack;
  struct sedProgram	*prog, **l_paren = NULL; // '{'
  struct sedCmd		*cmd;

  if (!first) {
    obstack_free(&obstack, NULL);
    return (NULL);
  }
  obstack_init(&obstack);
  prog = first->next = obstack_alloc(&obstack, sizeof(*prog));
  while ((cmd = &prog->cmd)->cmdChar = compile_cmd_address(cmd))
  {
    switch (cmd->cmdChar) { // Use switch as dispatch table
    case '#': vbuf_free(vbuf_readName()); 				continue;
    case '{': compile_lbrace; break;
    case '}': compile_rbrace; break;
    case 'a': case 'i': case 'c': cmd->info.text = read_text(); 	break;
    case 'q': case 'Q':                                    break; //nm ?
    case 'r': case 'R': case 'w': case 'W': cmd->info.text = vbuf_readName(); break;
    case ':': do_label(prog, NULL); 					break;
    case 'b': case 't': case 'T': do_label(NULL, prog); 		break;
    case 's': cmd->info.s = compile_s(); 				break;
    case 'y': compile_y(cmd);                                    	break;
    default:  if (!strchr("dDhHgGxlnNpPz=", cmd->cmdChar))
	        panic(cmd->cmdChar == EOF ? "Missing command" : "Unknown command '%c'",
		    cmd->cmdChar);
	      check_trailing_chars(cmd->cmdChar);
    }
    prog = prog->next = obstack_alloc(&obstack, sizeof(*prog)); // 'continue' skips this
  }
  prog->cmd.cmdChar = '#';
  do_label(NULL, NULL); // connect jmps
  return(prog->next = first);
}
