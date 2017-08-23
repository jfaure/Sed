#include <stdio.h>
#include "data.h"

char	nextChar()
{
  *g_in.cursor == '\n' && ++g_in.line;
  return (*g_in.cursor ? *g_in.cursor++ : nextProgStream());
}

int	nextNum(char c)
{
  int	r = 0;
  while (isdigit(c))
    r = r * 10 + c - '0', c = nextChar();
  c != EOF && prevChar(c);
  return (r);
}

int	nextNonBlank()
{
  char	c = nextChar();
  while (isblank(c))
    c = nextChar();
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
  if (g_in.info && g_in.info->filename) { // then try to read another line
    g_in.info->file || (g_in.info->file = xfopen(g_in.info->filename, "r"));
    g_in.info->alloc = 0;
    if (getline(&g_in.info->buf, &g_in.info->alloc, g_in.info->file) != -1) {
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

  if (!last) {
    last = g_in.info = xmalloc(sizeof(*last));
    g_in.line = 1;
  }
  else
    last = last->next = xmalloc(sizeof(*last));
  last->alloc = 0;
  last->next = 0;
  last->file = NULL;
  last->buf = str;
  last->filename = filename;
  g_in.cursor = g_in.info->buf;
}

char			compile_address_regex(regex_t *regex, char delim)
{
  char			in;
  struct vbuf		*text;
  int			cflags;

  cflags = REG_NOSUB | g_sedOptions.extended_regex_syntax * REG_EXTENDED;
  if (NULL == (text = snarf(delim, g_sedOptions.extended_regex_syntax ? 1 : -1)))
    panic("unterminated address regex (expected '%c')", delim);
  while ((in = nextChar()) == 'I' || in == 'M')
    cflags |= in == 'I' ? REG_ICASE : REG_NEWLINE;
  xregcomp(regex, text->buf, cflags);
  vbuf_del(text);
  return (in);
}

char			compile_address(struct sedAddr *addr, char in)
{
  if (in == '-' || in == '$' || isdigit(in)) {
    addr->type = ADDR_LINE;
    addr->info.line = in == '$' ? -1 : in == '-' ?
      -nextNum(nextChar()) : nextNum(in);
    in = nextChar();
    if (addr->info.line < g_lineInfo.lookahead)
      g_lineInfo.lookahead = addr->info.line;
  }
  else if (in == '/' || in == '\\') {
    addr->type = ADDR_REGEX;
    in == '\\' && (in = nextChar());
    in = compile_address_regex(&addr->info.regex, in);
    isblank(in) && (in = nextNonBlank());
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
  while ((in = nextNonBlank()) == ';' || in == '\n');
  if (in == EOF) return (0);
  in = compile_address(&cmd->addr->a1, in);
  if (cmd->addr->a1.type == ADDR_NONE && in != ',' && in != '~') {
    free(cmd->addr);
    cmd->addr = NULL;
    return (in);
  }
  if (in == ',') {
    cmd->addr->type = CMD_ADDR_RANGE;
    in = compile_address(&cmd->addr->a2, nextChar());
  }
  else if (in == '~') {
    cmd->addr->type = CMD_ADDR_STEP;
    in = nextChar();
    cmd->addr->step = cmd->addr->a2.info.line = nextNum(in);
    return (in);
  }
  else
    cmd->addr->type = CMD_ADDR_LINE;
  if (in == '!')
    cmd->addr->bang = 1, in = nextNonBlank();
  return (in);
}

void			compile_y(struct sedCmd *cmd)
{
  unsigned char		t;
  struct vbuf	*a, *b;

  a = snarf((t = nextChar()) == '/' ? t : (t = nextChar()), 0);
  b = snarf(t, 0);
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
  vbuf_del(a); vbuf_del(b);
}

char	get_s_options(struct SCmd *s)
{
  char	cflags, delim;

  cflags = REG_EXTENDED * g_sedOptions.extended_regex_syntax;
  s->g = s->p = s->e = s->d = s->number = cflags = 0; s->w = NULL;
  while (delim = nextChar())
    switch (delim) {
    case 'g': s->g = 1; break;
    case 'p': s->p = 1; !s->d && (s->d = !s->e); break;
    case 'e': s->e = 1; break;
    case 'm': case 'M': cflags |= REG_NEWLINE; break;
    case 'i': case 'I': cflags |= REG_ICASE; break;
    case '}': case '#': case ';': case ' ':  prevChar(); case '\n':
    case EOF: case '\r': return (cflags);
    default: panic("s: unknown option '%c'", delim);
    }
  // panic
}

char			**S_backrefs(struct SCmd *s, struct SReplacement *new, char *replace)
{
  int			i, len;

  new->text = replace;
  new->recipe = xmalloc(sizeof(*new->recipe) * (len = 10));
  i = 0;
  new->recipe[i++] = replace;
  for (; *replace; ++replace)
  {
    if (*replace == '&')
      new->recipe[i++] = 0, *replace = 0;
    else if (*replace == '\\') {
      *replace = 0;
      if (*++replace >= '0' && *replace <= '9')
	new->recipe[i++] = (char *)(long) *replace - '0';
      else panic("s backref: expected number, got '%c'", *replace);
    }
    else
      continue;
    if (replace[1] != '\\' && replace[1] != '&')
      new->recipe[i++] = replace + 1;
  }
  new->recipe[i] = (char *) -1; // mark end of recipe
  return (new->recipe);
}

struct SCmd 		*compile_s()
{
  struct SCmd		*s = xmalloc(sizeof(*s));
  char			delim = nextChar();
  struct vbuf		*replace, *regex;
  int			cflags;

  delim == '\\' && (delim = nextChar());
  if (!(regex = snarf(delim, g_sedOptions.extended_regex_syntax ? 1 : -1))
      || !(replace = snarf(delim, 0)))
    panic("unterminated delimiter %c", '/');
  cflags = get_s_options(s);
  s->new.text = replace->buf;
  s->new.n_refs = replace->len;
  s->new.recipe = S_backrefs(s, &s->new, s->new.text);
  free(replace);
  xregcomp(&s->pattern, regex->buf, cflags);
  vbuf_del(regex);
  return (s);
}

char			*read_label()
{
  char			in = nextChar();
  struct vbuf		*nm = vbuf_new();

  while (isblank(in))
    in = nextChar();
  while (in != EOF && in != ';' && in != '\n')
    vbuf_addChar(nm, in), in = nextChar();
  vbuf_addNull(nm);
  prevChar();
  return (vbuf_tostring(nm));
}

char			do_label(struct sedProgram *labelcmd, struct sedProgram *jmpcmd)
{
  static struct obstack obstack;
  static struct nameList { // private type
    char 			*name;
    struct nameList 		*next;
    struct sedProgram		*pos;
  } 				*labels = 0, *jumps = 0, *tmp;

  if (!labels && !jumps)
    obstack_init(&obstack);
  if (labelcmd) { // add label
    tmp = labels;
    labels = obstack_alloc(&obstack, sizeof(*labels));
    labels->name = read_label();
    labels->next = tmp;
    labels->pos = labelcmd;
  }
  else if (jmpcmd) {// add jmp
    tmp = jumps;
    jumps = obstack_alloc(&obstack, sizeof(*labels));
    jumps->next = tmp;
    jumps->name = read_label();
    jumps->pos = jmpcmd;
  }
  else { // finished, connect all jumps
    for ( ; jumps; jumps = jumps->next)
      for (tmp = labels; tmp; tmp = tmp->next)
	if (!strcmp(tmp->name, jumps->name)) {
	  jumps->pos->cmd.info.jmp = tmp->pos; 
	  break;
	}
    obstack_free(&obstack, NULL);
  }
  return (0);
}

#define compile_lbrace if (l_paren) panic("Too many '{'");\
                     else l_paren = &prog->cmd.info.jmp, \
		       prog->cmd.addr->bang = !prog->cmd.addr->bang;

#define compile_rbrace if (!l_paren) panic("unexpected '}'");\
  		     else *l_paren = prog; l_paren = NULL;

struct sedProgram	*compile_program(struct sedProgram *const first)
{
  static struct obstack	obstack;
  struct sedProgram	*prog = first, **l_paren = NULL; // '{'
  struct sedCmd		*cmd;
  char			chk;

  if (!first) {
    obstack_free(&obstack, NULL);
    return (NULL);
  }
  obstack_init(&obstack);
  prog = first->next = obstack_alloc(&obstack, sizeof(*prog));
  while ((cmd = &prog->cmd)->cmdChar = compile_cmd_address(cmd)) {
    switch (cmd->cmdChar) {
    case '#': vbuf_del(vbuf_readName()); 				continue;
    case '{': compile_lbrace; 						break;
    case '}': compile_rbrace; 						break;
    case 'a': case 'i': case 'c': cmd->info.text = read_text(); 	break;
    case 'q': case 'Q': cmd->info.int_arg = nextNum(nextChar());        break;
    case 'r': case 'R': case 'w': case 'W':
	cmd->info.text = vbuf_readName(); 				break;
    case ':': do_label(prog, NULL); 					break;
    case 'b': case 't': case 'T': do_label(NULL, prog);  	 	break;
    case 's': cmd->info.s = compile_s(); 				break;
    case 'y': compile_y(cmd);                                    	break;
    default:  if (!strchr("dDhHgGxlnNpPz=eF", cmd->cmdChar))
	        panic(cmd->cmdChar == EOF ? "Missing command" : "Unknown command '%c'",
		    cmd->cmdChar);
    }
    if ((chk = cmd->cmdChar) != '{')
      if ((chk = nextNonBlank()) == '}')
	prevChar();
      else if (!strchr(";\n", chk) && chk != EOF)
	panic("%c: unknown argument '%c'", cmd->cmdChar, chk);
    prog = prog->next = obstack_alloc(&obstack, sizeof(*prog));
  }
  prog->cmd.cmdChar = '#';
  do_label(prog, NULL); // add label for empty branches
  do_label(NULL, NULL); // connect jmps
  return(prog->next = first);
}
