#include "data.h"

bool		match_address(addr, pattern)
  struct sedAddr *addr; struct sedLine *pattern;
{  
  return ((addr->type == ADDR_NONE 
    || addr->type == ADDR_LINE &&
      (g_lineInfo.current == addr->info.line || g_lineInfo.current == g_lineInfo.lookahead)
    || addr->type == ADDR_REGEX
    && !regexec(&addr->info.regex, pattern->active, 0, NULL, 0)));
}

# define addressIfRangeInactive(addr, pattern)		\
  if (addr->a1.type != ADDR_NONE) // range inactive	\
    if (match_address(&addr->a1, pattern)) {		\
      addr->a1.type = ADDR_NONE; // activate range 	\
      return (true);					\
    }							\
    else						\
      return (false);
 
bool		match_addressRange(addr, pattern)
  struct sedCmdAddr *addr; struct sedLine *pattern;
{
  addressIfRangeInactive(addr, pattern);
  // range active
  if (match_address(&addr->a2, pattern))
    addr->type = CMD_ADDR_DONE;  // Address will never be matched again.. (remove cmd ?)
  return (true);
}

bool		match_addressStep(addr, pattern)
  struct sedCmdAddr *addr; struct sedLine *pattern;
{
  addressIfRangeInactive(addr, pattern);
  // step active
  --addr->step || (addr->step = addr->a2.info.line);
  return (addr->step == addr->a2.info.line);
}

bool		match_cmdAddress(prog, addr, pattern)
    struct sedProgram *prog; struct sedCmdAddr *addr; struct sedLine *pattern;
{
  return (addr->type != CMD_ADDR_DONE && addr->bang !=
      (  addr->type == CMD_ADDR_LINE  && match_address(&addr->a1,  pattern)
      || addr->type == CMD_ADDR_RANGE && match_addressRange(addr, pattern)
      || addr->type == CMD_ADDR_STEP  && match_addressStep(addr, pattern)));
}

#define AVG_MAX_LINE 100

void		sedLine_init(struct sedLine *l)
{
  l->active = l->buf = xmalloc(l->alloc = AVG_MAX_LINE);
  l->buf[l->len = 0] = 0;
  l->chomped = true;
}

struct sedLine		*sedLine_new()
{
  struct sedLine	*l = xmalloc(sizeof(*l));
 
  sedLine_init(l);
  return (l);
}

void			sedLine_appendText(struct sedLine *l, char *text, int len)
{
  if (l->alloc - l->len < len + 2)
    l->active = l->buf = xrealloc(l->buf, l->alloc + l->len + len);
  memcpy(l->active + l->len, text, len);
  l->active[l->len += len] = 0;
}

ssize_t			sedLine_getline(struct sedLine *line, FILE *in)
{
  ssize_t		r = getline(&line->buf, &line->alloc, in);

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

  l->chomped = true;
  if (!last) { // initialize and read -g_lineInfo.lookahead lines into list
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
    if (last->next == last) {
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

void			sedLine_printLine(struct sedLine *l, FILE *out)
{
  fputs(l->active, out);
  l->chomped == true && fputc('\n', out);
}

void			sedLine_printEmbeddedLine(struct sedLine *l, FILE *out)
{
  fwrite(l->active, strchrnul(l->active, '\n') - l->active, 1, out);
  l->chomped == true && fputc('\n', out);
}

void			sedLine_deleteEmbeddedLine(struct sedLine *l, FILE *out)
{
  char			*save = l->active;

  l->active = strchrnul(l->active, '\n');
  *l->active == '\n' && ++l->active;
  l->len -= l->active - save;
  if (out && l->len) {
    fwrite(l->active, l->active - save, 1, out);
    if (l->chomped)
      fputs("\n", out);
  }
}

void			sedLine_deleteLine(struct sedLine *l, FILE *out)
{
  if (out)
    sedLine_printLine(l, out);
  (l->active = l->buf)[l->len = 0] = 0;
}

void			sedLine_appendLineList(struct sedLine *line, struct sedLine *ap)
{
  sedLine_appendText(line, "\n", 1);
  sedLine_appendText(line, ap->active, ap->len);
}

void			exec_l(char *text, FILE *out)
{
  for (;*text; ++text)
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
  char		**recipe = s->new.recipe;

  for (new->len = 0; *recipe != (char *) -1; ++recipe)
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
  char			*cursor = pattern->active;
  struct vbuf		*new = vbuf_new();
  regmatch_t		pmatch[10];
  int			diff, subs = 0;

  do {
    if (regexec(&s->pattern, cursor, 10, pmatch, 0))
      break;
    ++subs;
    new = resolve_backrefs(new, s, pmatch, cursor);
    diff = pattern->len - (cursor - pattern->buf + pmatch[0].rm_so);
    assert(diff >= 0);
    while (2 + diff > pattern->alloc - pattern->len) {
      pattern->active -= (long) pattern->buf; cursor -= (long) pattern->buf;
      pattern->buf = xrealloc(pattern->buf, pattern->alloc <<= 1);
      pattern->active += (long) pattern->buf; cursor += (long) pattern->buf;
    }
    memmove(cursor + pmatch[0].rm_so + new->len, cursor + pmatch[0].rm_eo, diff);
    memmove(cursor + pmatch[0].rm_so, new->buf, new->len);
    pattern->len += new->len - (pmatch[0].rm_eo - pmatch[0].rm_so);
    cursor += pmatch[0].rm_so + new->len;
  } while ((const char) s->g && *cursor);
  vbuf_del(new);
  return (subs); // how many ? (only >1 if g option)
}

char			exec_file(struct sedProgram *const first, FILE *in, FILE *out,
				  char const *const filename)
{
  char			lastsub = 0; // successful s since t or T ?
  struct sedProgram	*prog = first;
  struct sedLine 	pattern, hold, x;
  struct vbuf		a_cmd;
  struct sedCmd		*cmd;
  FILE 			*file;

  sedLine_init(&pattern); sedLine_init(&hold); vbuf_init(&a_cmd);
re_cycle:
  while (sedLine_readLine(&pattern, in) == true) {
    while ((prog = prog->next) != first && (cmd = &prog->cmd))
      if (!cmd->addr || match_cmdAddress(prog, cmd->addr, &pattern))
	switch (cmd->cmdChar) {
	case '#': case ':': case '}': 						break;
        case '{': prog = cmd->info.jmp; 					continue;
        case '=': fprintf(out, "%d\n", g_lineInfo.current); 			break;
        case 'a': vbuf_addString(a_cmd, cmd->info.text->buf, cmd->info.text->len);	break;
        case 'i': fwrite(cmd->info.text->buf, cmd->info.text->len, 1, out); 	break;
        case 'q': sedLine_deleteLine(&pattern, g_sedOptions.silent ? NULL :  out); //fall
        case 'Q': return (cmd->info.int_arg);
        case 'c': fwrite(cmd->info.text->buf, cmd->info.text->len, 1, out); // fall
	case 'd': d: sedLine_deleteLine(&pattern, NULL); prog = first; 		goto re_cycle;
        case 'D': sedLine_deleteEmbeddedLine(&pattern, NULL); pattern.chomped = false; break;
        case 'h': sedLine_deleteLine(&hold, 0); 				//fall
        case 'H': sedLine_appendLineList(&hold, &pattern); 			break;
        case 'g': sedLine_deleteLine(&pattern, 0); 				//fall
        case 'G': sedLine_appendLineList(&pattern, &hold); 			break;
	case 'l': exec_l(pattern.buf, out); 					goto d;
        case 'n': sedLine_deleteLine(&pattern, out); sedLine_readLine(&pattern, in); 	continue;
        case 'N': sedLine_readLine(&pattern, in); 				break;
        case 'p': sedLine_printLine(&pattern, out); 				break;
        case 'P': sedLine_printEmbeddedLine(&pattern, out);			break;
        case 's': lastsub |= exec_s(cmd->info.s, &pattern); 			break;
 	case 'b': prog = cmd->info.jmp;						continue;
        case 'T': lastsub || (prog = cmd->info.jmp); lastsub = 0;		//fall
        case 't': lastsub && (prog = cmd->info.jmp); lastsub = 0; 		continue;
	case 'e': file = popen(pattern.active, "w"); pclose(file); 		goto d;
	case 'F': fputs(filename, out);						break;
	case 'r':
        case 'R':
        case 'w':
        case 'W': panic("not done"); 						break;
        case 'x': x = pattern; pattern = hold; hold = x; 			break;
        case 'z': sedLine_deleteLine(&pattern, 0); 				break;
        case 'y': for (char *p = pattern.active; p <= pattern.active + pattern.len; ++p)
	            cmd->info.y[*p] && (*p = cmd->info.y[*p]); 			break;
        default : panic("Internal error: compiled '%c'", cmd->cmdChar, cmd->cmdChar);
	}
    sedLine_deleteLine(&pattern, g_sedOptions.silent ? NULL : out);
    a_cmd.len && fputs(a_cmd.buf, out); a_cmd.len = 0; // flush append_queue;
  }
  free(a_cmd.buf);
  return (0);
}

char		exec_stream(struct sedProgram *prog, char **filelist)
{
  int		st;
  FILE		*file;

  if (!*filelist)
    st = exec_file(prog, stdin, stdout, "-");
  while (*filelist) {
    g_sedOptions.separate && (g_lineInfo.current = 0);
    file = **filelist == '-'  && !filelist[0][1] ? stdin : fopen(*filelist, "r");
    if (!file)
      printf("Couldn't open file '%s' for reading\n", *filelist);
    else {
      st |= exec_file(prog, file, stdout, *filelist);
      file != stdin && fclose(file);
    }
    ++filelist;
  }
  return (st);
}
