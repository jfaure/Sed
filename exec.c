#include "data.h"

bool			match_address(addr, pattern)
  struct sedAddr *addr; struct sedLine *pattern;
{  
  return ((addr->type == ADDR_NONE 
    || addr->type == ADDR_LINE &&
      (g_lineInfo.current == addr->info.line || g_lineInfo.current == g_lineInfo.lookahead)
    || addr->type == ADDR_END   && g_lineInfo.lookahead == g_lineInfo.current 
    || addr->type == ADDR_REGEX
    && !regexec(&addr->info.regex, pattern->active, 0, NULL, 0)));
}

bool			match_addressRange(addr, pattern)
  struct sedCmdAddr *addr; struct sedLine *pattern;
{
  bool			r;

  if (addr->a1.type != ADDR_NONE) // range inactive
    if (r = match_address(&addr->a1, pattern))
    {
      addr->a1.type = ADDR_NONE; // activate range 
      return (true);
    }
    else
      return (false);
  // range active
  if (r = match_address(&addr->a2, pattern))
    addr->type = CMD_ADDR_DONE;  // Address will never be matched again.. (remove cmd ?)
  return (true);
}

bool			match_addressStep(addr, pattern)
  struct sedCmdAddr *addr; struct sedLine *pattern;
{
}

bool			match_cmdAddress(prog, addr, pattern)
    struct sedProgram *prog; struct sedCmdAddr *addr; struct sedLine *pattern;
{
  bool			match;

  if (addr->type == CMD_ADDR_DONE)
    return (false);
  return (addr->bang != 
      (  addr->type == CMD_ADDR_LINE  && match_address(&addr->a1,  pattern)
      || addr->type == CMD_ADDR_RANGE && match_addressRange(addr, pattern)
      || addr->type == CMD_ADDR_STEP  && match_addressStep(addr, pattern)));
  return (addr->bang += match);
}

void	append_queue(struct vbuf *text, FILE *out)
{
  static struct vbuf *buf;

  if (text)
    vbuf_addString(buf ? buf : (buf = vbuf_new()), text->buf, text->len);
  else if (buf)
    {
      buf->len && fwrite(buf->buf, buf->len, 1, out);
      vbuf_free(buf);
    }
}

#define AVG_MAX_LINE 100 

struct sedLine		*sedLine_new()
{
  struct sedLine	*l;

  l = xmalloc(sizeof(*l));
  l->active = l->buf = xmalloc(l->alloc = AVG_MAX_LINE);
  l->buf[l->len = 0] = 0;
  l->chomped = true;
  return (l);
}

void			sedLine_appendText(struct sedLine *l, char *text, int len)
{
  if (l->alloc - l->len < len + 2)
    l->active = l->buf = xrealloc(l->buf, l->alloc + l->len + len);
  memcpy(l->active + l->len, text, len);
  l->len += len;
  l->active[l->len] = 0;
}

ssize_t			sedLine_getline(struct sedLine *line, FILE *in)
{
  ssize_t		r;

  r = getline(&line->buf, &line->alloc, in);
  if (r >= 0)
    if (line->buf[(line->len = r) - (r != 0)] == '\n')
      line->buf[--line->len] = 0;
    else
      line->chomped = false; // last line
  return (r);
}

/*
** Read a line of input, maintain lookahead (if script needs it) eg. addresses: '$' and '-5'
*/
bool			sedLine_readLine(struct sedLine *l, FILE *in)
{
  static struct sedLinelist { // Circular list of lookahead lines (last->next == first)
    struct sedLine	*line;
    struct sedLinelist	*next;
  } 			*last = NULL, *t;
  int			i = 0;

  if (!last) { // initialize
    if (feof(in))
      return (false);
    last = t = xmalloc(sizeof(*last));
    for (i = g_lineInfo.lookahead; i < 0; ++i) {
      last->line = sedLine_new();
      if (-1 == sedLine_getline(last->line, in)) {
	g_lineInfo.lookahead = i;
	break;
      }
      last = last->next = xmalloc(sizeof(*last));
    }
    last->line = sedLine_new();
    last->next = t;
  }
  if (feof(in) || -1 == sedLine_getline(last->line, in)) { // read line into last
    if (g_lineInfo.lookahead < 0)
      g_lineInfo.lookahead = -g_lineInfo.lookahead + 1;
    if (last->next == last)
    {
      free(last);
      last = NULL;
      return (false);
    }
    --g_lineInfo.lookahead;
    t = last->next;
    last->line = t->line;
    last->next = t->next;
    free(t);
    t = last;
  }
  else
    t = last->next;
  ++g_lineInfo.current;
  sedLine_appendText(l, t->line->buf, t->line->len);
  last = t;
 return (true);
}

void			sedLine_deleteEmbeddedLine(struct sedLine *l, FILE *out)
{
  char			*save;

  save = l->active;
  l->active = strchrnul(l->active, '\n');
  l->active += *l->active == '\n';
  l->len -= l->active - save;
  if (out && l->len)
  {
    fwrite(l->active, l->active - save, 1, out);
    if (l->chomped)
      fputs("\n", out);
  }
}

void			sedLine_appendLineList(struct sedLine *line, struct sedLine *ap)
{
  sedLine_appendText(line, ap->active, ap->len);
}

void			sedLine_deleteLine(struct sedLine *l, FILE *out)
{
  if (out)
  {
    fwrite(l->active, l->len, 1, out);
    if (l->chomped == true)
      fputc('\n', out);
  }
  l->active = l->buf;
  l->buf[l->len = 0] = 0;
}

void			exec_l(char *text, FILE *out)
{
  panic("fix l");
  while (1)
    switch (*text) {
    case '\a': fputs("\\a", out); break;
    case '\b': fputs("\\b", out); break;
    case '\f': fputs("\\f", out); break;
    case '\n': fputs("\\n", out); break;
    case '\r': fputs("\\r", out); break;
    case '\t': fputs("\\t", out); break;
    case '\v': fputs("\\v", out); break;
    default: fputc(*text, out);
    }
}

struct vbuf		*resolve_backrefs(struct vbuf *new, struct SCmd *s,
    regmatch_t pmatch[], char *cursor)
{
  char		**recipe;

  new->len = 0;
  for (recipe = s->new.recipe; *recipe != (char *) -1; ++recipe) // \n\n breaks '$'
    if ((long) *recipe < 10)
      vbuf_addString(new, cursor + pmatch[(long) *recipe].rm_so,
	  pmatch[(long) *recipe].rm_eo - pmatch[(long) *recipe].rm_so);
    else
      vbuf_addString(new, *recipe, strlen(*recipe));
  vbuf_addNull(new);
  return (new);
}

char			exec_s(struct SCmd *s, struct sedLine *pattern)
{
  char			*cursor;
  struct vbuf		*new;
  regmatch_t		pmatch[10];
  int			diff, subs;

  new = vbuf_new();
  cursor = pattern->active;
  subs = 0;
  do {
    if (regexec(&s->pattern, cursor, 10, pmatch, 0)) {
      vbuf_free(new);
      break;
    }
    ++subs;
    new = resolve_backrefs(new, s, pmatch, cursor);
    diff = pattern->len - (cursor - pattern->buf + pmatch[0].rm_so);
    while (2 + diff > pattern->alloc - pattern->len)
    {
      pattern->active -= (long) pattern->buf; cursor -= (long) pattern->buf;
      pattern->buf = xrealloc(pattern->buf, pattern->alloc <<= 1);
      pattern->active += (long) pattern->buf; cursor += (long) pattern->buf;
    }
    assert(diff > 0);
    memmove(cursor + pmatch[0].rm_so + new->len, cursor + pmatch[0].rm_eo, diff);
    memmove(cursor + pmatch[0].rm_so, new->buf, new->len);
    pattern->len += new->len - (pmatch[0].rm_eo - pmatch[0].rm_so);
    cursor += pmatch[0].rm_so + new->len;
  } while ((const char) s->g && *cursor);
  return (subs); // has a substitution been made
}

char			exec_file(struct sedProgram *const first, FILE *in, FILE *out) 
{ 
  struct sedProgram		*prog;
  static struct sedLine 	*pattern, *hold, *x;
  static char			lastsub;
  struct sedCmd			*cmd;

  prog = first;
  lastsub = 0; pattern = sedLine_new(); hold = sedLine_new();
re_cycle:
  while (sedLine_readLine(pattern, in) == true)
  {
    while ((prog = prog->next) != first && (cmd = &prog->cmd))
      if (!cmd->addr || match_cmdAddress(prog, cmd->addr, pattern))
	switch (cmd->cmdChar) {
	case '#': case ':': case '}': break;
        case '{': prog = cmd->info.jmp; continue;
        case '=': fprintf(out, "%d\n", g_lineInfo.current); fflush(out); break;
        case 'a': append_queue(cmd->info.text, out); break;
        case 'i': fwrite(cmd->info.text->buf, cmd->info.text->len, 1, out); break;
        case 'q': sedLine_deleteLine(pattern, g_sedOptions.silent ? NULL :  out);
        case 'Q': return (cmd->info.int_arg);
        case 'r':
        case 'R':
        case 'c': fwrite(cmd->info.text->buf, cmd->info.text->len, 1, out); // fallthrough
	case 'd': sedLine_deleteLine(pattern, NULL); prog = first; goto re_cycle;
        case 'D': sedLine_deleteEmbeddedLine(pattern, NULL); break;
        case 'h': sedLine_deleteLine(hold, 0);
	          sedLine_appendLineList(hold, pattern); break;
        case 'H': sedLine_appendLineList(hold, pattern); break;
        case 'g': sedLine_deleteLine(pattern, 0);
	          sedLine_appendLineList(pattern, hold);break;
        case 'G': sedLine_appendText(pattern, "\n", 1); sedLine_appendLineList(pattern, hold); break;
	case 'l': exec_l(pattern->buf, out); sedLine_deleteLine(pattern, 0); goto re_cycle;
        case 'n': sedLine_deleteLine(pattern, out); sedLine_readLine(pattern, in); continue;
        case 'N': sedLine_appendText(pattern, "\n", 1); sedLine_readLine(pattern, in); break;
        case 'p': sedLine_deleteLine(pattern, out); break;
        case 'P': sedLine_deleteEmbeddedLine(pattern, out); break;
        case 's': lastsub = exec_s(cmd->info.s, pattern); break;
 	case 'b': prog = cmd->info.jmp; continue; // if no jmp address ?
        case 'T': lastsub || (prog = cmd->info.jmp); lastsub = 0;
        case 't': lastsub && (prog = cmd->info.jmp); lastsub = 0; continue;
        case 'w':
        case 'W':
        case 'x': x = pattern; pattern = hold; hold = x; break;
        case 'z': sedLine_deleteLine(pattern, 0); break;
        case 'y': for (char *p = pattern->active; p <= pattern->active + pattern->len; ++p)
	            cmd->info.y[*p] && (*p = cmd->info.y[*p]); break;
        default : panic("Internal error: compiled '%c'(%d)", cmd->cmdChar, cmd->cmdChar);
	}
    sedLine_deleteLine(pattern, g_sedOptions.silent ? NULL : out);
    append_queue(0, out); // flush append_queue;
  }
  return (0);
}

char			exec_stream(struct sedProgram *prog, char **filelist)
{
  int			st;
  FILE			*file;

  st = 0;
  if (!*filelist)
    st = exec_file(prog, stdin, stdout);
  while (*filelist)
  {
    g_sedOptions.separate && (g_lineInfo.current = 0);
    file = **filelist == '-'  && !filelist[0][1] ? stdin : fopen(*filelist, "r");
    if (!file)
      printf("Couldn't open file '%s' for reading\n", *filelist);
    else
    {
      st |= exec_file(prog, file, stdout);
      file != stdin && fclose(file);
    }
    ++filelist;
  }
  return (st);
}
