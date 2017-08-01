#ifndef DATA_H_
# define DATA_H_

# define _GNU_SOURCE

#include <unistd.h>
#include <getopt.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h> //temporary ?
#include <ctype.h>

#include <obstack.h>
#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free  free

#define DBcompile //printf

struct		sedOptions	{
  unsigned	silent:			1;
  unsigned	follow_symlinks:	1;
  unsigned	in_place:		1;
  unsigned	null_data:		1;
  unsigned	separate:		1;
  unsigned	extended_regex_syntax:	1;
} 		g_sedOptions;

char asdf;
#define nextChar() ((asdf = *g_in.cursor == 0 ? \
      nextProgStream() : *g_in.cursor++) \
    + !DBcompile("nextChar '%c'\n", asdf))

#define prevChar(c) assert(asdf = \
    g_in.cursor >= g_in.next->buf ? \
    *g_in.cursor : g_in.last\
    + !DBcompile("prevChar '%c'\n", asdf))

struct		zbuf	{
  char		*cursor;
  char		last;
  struct zbuflist	{
    char		*buf;
    size_t		alloc;
    char		*filename;
    FILE		*file;
    struct zbuflist	*next;
  }		*next;
}		g_in;

struct		vbuf	{// vector buffers grow with xrealloc
  char		*buf;
  ssize_t	len;
  ssize_t	alloc;
};

/*
** Data for pattern and holdspace. extended vbuf, since
** we must be able to call regexec on an entire line.
** $ address is tricky, and must be handled on the fly
*/
struct			lineList	{
  char			*buf;
  size_t		alloc;
  char			*active;
  size_t		len;
  struct vbuf		*lookahead;
};

enum sedAddrType	{
  ADDR_NONE, // allow cmd adresses like '1,' and ',9'
  ADDR_LINE,
  ADDR_END, // '$'
  ADDR_REGEX,
};

struct			sedAddr	{
  enum sedAddrType	type; // basically regex or line_no
  union	{
    int			line;
    regex_t		regex;
  }			info;
};

enum sedCmdAddrType	{ // do we use a1, a2, step ?
  CMD_ADDR_DONE,  // address will never match again
  CMD_ADDR_LINE, // use only a1
  CMD_ADDR_RANGE, // use a1, a2 
  CMD_ADDR_STEP, // use a1, a2, step
};

struct			sedCmdAddr	{
// handle 1 or 2 sedAddr, steps '~' and non_matches '!'.
  unsigned		bang:	1; // '!'
  enum sedCmdAddrType	type;
  struct sedAddr	a1, a2;
  size_t		step;
};

struct 			sedCmd	{
  struct sedCmdAddr	*addr; // NULL matches unconditionally
  char			cmdChar;
  union		{
    struct SCmd		*s;
    char		*y;
    struct vbuf		*text; // For a, i, c commands
    int			int_arg;
    FILE		*file;
    struct sedProgram	*jmp;
  }			info;
};

/* 
** a sedProgram is a circular list of sedCmds in an obstack -
** to reduce pagefaults, fragmentation and del cmds quick
** 'first' contains no cmd info 
*/
struct			sedProgram	{
  struct sedProgram	*next;
  struct sedCmd		cmd;
};

/* 
** concatenate recipe to create the replacement.
** pointer values 0 to 9 are backreferences
*/
struct SReplacement		{
  char			*text; //private reference string
  size_t 		n_refs;
  char			**recipe; //parts of text, backrefs
};

struct			SCmd	{
  regex_t		pattern;
  struct SReplacement	new;
  unsigned		g:	1;
  unsigned		p:	1;
  unsigned		e:	1;
  unsigned		d:	1; // debug: print before eval
  FILE			*w; // options i and m affect regex compiling
  int			number;
};

void	*xmalloc(size_t len);
void	*xrealloc(void *ptr, size_t len);
FILE	*xfopen(const char *f_name, const char *mode);

struct vbuf	*vbuf_new();
ssize_t	vbuf_getline(struct vbuf *text, FILE *in);
struct vbuf	*read_text();
struct vbuf	*snarf(char delim);
struct vbuf	*vbuf_readName();
char		*vbuf_tostring(struct vbuf *);

struct lineList	*lineList_new();
void		lineList_appendText(struct lineList *l, char *text, int len);
void		lineList_deleteEmbeddedLine(struct lineList *l, FILE *out);
void		lineList_appendLineList(struct lineList *line, struct lineList *ap);
void		lineList_deleteLine(struct lineList *l, FILE *out);

struct sedProgram	*compile_file(struct sedProgram *compile, const char *f_name);
struct sedProgram	*compile_string(struct sedProgram *compile, char *str);
char	exec_stream(struct sedProgram *prog, char **files);

#endif // ifndef DATA_H_
