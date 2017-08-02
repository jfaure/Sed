#include "data.h"

int			g_lineCount;

bool			match_address(addr, pattern, line)
  struct sedAddr *addr; struct lineList *pattern; int line;
{
  return ((addr->type == ADDR_NONE 
    || addr->type == ADDR_LINE  && line == addr->info.line
    || addr->type == ADDR_END   && !pattern->lookahead
    || addr->type == ADDR_REGEX
    && !regexec(&addr->info.regex, pattern->active, 0, NULL, 0)));
}

bool			match_addressRange(addr, pattern, line)
  struct sedCmdAddr *addr; struct lineList *pattern; int line;
{
  bool			r;

  if (addr->a1.type != ADDR_NONE) // Range inactive
    if (r = match_address(addr->a1, pattern, line))
    {
      addr->a1.type = ADDR_NONE; // Range active
      return (true);
    }
  if (r = match_address(addr->a2, pattern, line))
  {
    addr->type = CMD_ADDR_DONE;  // Address will never be matched again.. (remove cmd ?)
    return (false);
  }
  return (true);
}

bool			match_addressStep(addr, pattern, line)
  struct sedCmdAddr *addr; struct lineList *pattern; int line;
{
}

// Wil incidentally remove dead addresses from the program
bool			match_cmdAddress(prog, addr, pattern, line)
    struct sedProgram *prog; struct sedCmdAddr *addr; struct lineList *pattern; int line; 
{
  bool			match;

  return (addr->bang != 
      (addr->type == CMD_ADDR_DONE
      || addr->type == CMD_ADDR_LINE  && match_address(&addr->a1,  pattern, line)
      || addr->type == CMD_ADDR_RANGE && match_addressRange(addr, pattern, line)
      || addr->type == CMD_ADDR_STEP  && match_addressStep(addr, pattern, line)));
  return (addr->bang += match);
}

void	append_queue(struct vbuf *text, FILE *out)
{
  static struct vbuf buf = {NULL, 0, 0};

  if (!text)
  {
    if (buf.len)
      fwrite(buf.buf, buf.len, 1, out);
    return ;
  }
  if (buf.alloc < text->len + buf.len)
  {
    buf.alloc || (buf.alloc = 512);
    while (buf.alloc < text->len + buf.len)
      buf.alloc <<= 1;
    buf.buf = xrealloc(buf.buf, buf.alloc);
  }
  memcpy(buf.buf + buf.len, text->buf, text->len);
  buf.len += text->len;
}

struct lineList		*lineList_new()
{
  struct lineList	*l;

  l = xmalloc(sizeof(*l));
  l->active = l->buf = xmalloc(l->alloc = 256);
  l->buf[l->len = 0] = 0;
  l->lookahead = vbuf_new();
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
  if (vbuf_getline(l->lookahead, in) == -1)
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

struct vbuf		*resolve_backrefs(struct vbuf *new, struct SCmd *s,
    regmatch_t pmatch[], char *cursor)
{
  char		**recipe;

  new->len = 0;
  for (recipe = s->new.recipe; *recipe != (char *) -1; ++recipe) // \n\n breaks '$'
    if ((long) *recipe < 10)
    {
      vbuf_addString(new, cursor + pmatch[(long) *recipe].rm_so,
	  pmatch[(long) *recipe].rm_eo - pmatch[(long) *recipe].rm_so);
    }
    else
      vbuf_addString(new, *recipe, strlen(*recipe));
  vbuf_addNull(new);
  return (new);
}

char			exec_s(struct SCmd *s, struct lineList *pattern)
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

char			exec_file(struct sedProgram *prog, FILE *in, FILE *out) 
{ 
  struct sedProgram		*first;
  static struct lineList 	*pattern, *hold;
  static char			lastsub;
  struct sedCmd			*cmd;

  first = prog;
  lastsub = 0; pattern = lineList_new(); hold = lineList_new();
  while (lineList_readLine(pattern, in) == true)
  {
    while ((prog = prog->next) != first && (cmd = &prog->cmd))
      if (!cmd->addr || match_cmdAddress(prog, cmd->addr, pattern, g_lineCount))
	switch (cmd->cmdChar) {
	case '#': case ':': break;
        case '=': fprintf(out, "%d\n", g_lineCount); fflush(out); break;
        case 'a': append_queue(cmd->info.text, out); break;
        case 'i': fwrite(cmd->info.text->buf, cmd->info.text->len, 1, out); break;
        case 'q': lineList_deleteLine(pattern, g_sedOptions.silent ? NULL :  out);
        case 'Q': return (cmd->info.int_arg);
        case 'r':
        case 'R':
        case 'c': fwrite(cmd->info.text->buf, cmd->info.text->len, 1, out); // fallthrough
        case 'd': lineList_deleteLine(pattern, NULL); break;
        case 'D': lineList_deleteEmbeddedLine(pattern, NULL); break;
        case 'h': lineList_deleteLine(hold, 0);
	          lineList_appendLineList(hold, pattern); break;
        case 'H': lineList_appendLineList(hold, pattern); break;
        case 'g': lineList_deleteLine(pattern, 0);
	          lineList_appendLineList(pattern, hold);break;
        case 'G': lineList_appendLineList(pattern, hold); break;
        case 'l':
        case 'n': lineList_readLine(pattern, in); continue;
        case 'N': lineList_readLine(pattern, in); break;
        case 'p': lineList_deleteLine(pattern, out); break;
        case 'P': lineList_deleteEmbeddedLine(pattern, out); break;
        case 's': lastsub = exec_s(cmd->info.s, pattern); break;
 	case 'b': prog = cmd->info.jmp; continue; // if no jmp address ?
        case 'T': lastsub || (prog = cmd->info.jmp); lastsub = 0;
        case 't': lastsub && (prog = cmd->info.jmp); lastsub = 0; continue;
        case 'w':
        case 'W':
        case 'x':
        case 'y': for (char *p = pattern->active; p <= pattern->active + pattern->len; ++p)
	            cmd->info.y[*p] && (*p = cmd->info.y[*p]); break;
        default : panic("Internal error: compiled '%c'(%d)", cmd->cmdChar, cmd->cmdChar);
	}
end:
    lineList_deleteLine(pattern, g_sedOptions.silent ? NULL : out);
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
    g_sedOptions.separate && (g_lineCount = 0);
    printf("executing file '%s'\n", *filelist);
    file = **filelist == '-'  && !filelist[0][1] ? stdin : fopen(*filelist, "r");
    if (!file)
      printf("Couldn't open file '%s' for reading\n", *filelist);
    else
    {
      st |= exec_file(prog, file, stdout);
      fclose(file);
    }
    ++filelist;
  }
  return (st);
}
