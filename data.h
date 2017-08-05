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

struct sedRuntime {
  int			current;
  int			lookahead;
  // '$' address == -1, 2n'd last == -2 etc..
  struct vbuf		*str_output;
}			g_lineInfo;

struct		zbuf	{
  int		line;
  char		*cursor;
  char		last;
  struct zbuflist	{
    char		*buf;
    size_t		alloc;
    char		*filename;
    FILE		*file;
    struct zbuflist	*next;
  }		*info;
}		g_in;

char	nextChar();
char	prevChar(char c);
char	nextProgStream();

#define mnextChar() (*g_in.cursor == 0 ? \
      nextProgStream() : *g_in.cursor++)

#define mprevChar(c) *--g_in.cursor // not 100% safe

struct		vbuf	{// vector buffers grow by xrealloc
  char		*buf;
  ssize_t	len;
  ssize_t	alloc;
};

/*
** struct sedLine:
** Data for pattern and holdspace - contiguous in memory,
** since we must sometimes call regexec on the entire region.
** line sizes are heavily affected by number of N,G,H cmds
*/
struct			sedLine	{
  char			*buf;
  size_t		alloc;
  char			*active;
  size_t		len;
  bool			chomped; // almost always true
};

/* 
** ----------- Compiled Script datatypes -----------
*/

struct			sedAddr	{ // details for sedCmdAddr
  enum sedAddrType	{
    ADDR_NONE, // for cmd adresses like '1,' and ',9'
    ADDR_LINE,
    ADDR_END, // '$'
    ADDR_REGEX,
  }			type;
  union	{
    int			line;
    regex_t		regex;
  }			info;
};

struct			sedCmdAddr	{
  enum sedCmdAddrType	{ // do we use a1, a2, step ?
    CMD_ADDR_DONE,  // address will never match again
    CMD_ADDR_LINE, // use only a1
    CMD_ADDR_RANGE, // use a1, a2 
    CMD_ADDR_STEP, // use a1, a2, step
  } 			type;
  unsigned		bang:	1; // '!'
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
    struct sedProgram	*jmp; // 'b' 't' 'T' cmds
  }			info;
};

/* 
** a sedProgram is a circular list of sedCmds in an obstack -
** to reduce pagefaults and fragmentation, and del cmds quick
** first entry contains no cmd info 
*/
struct			sedProgram	{
  struct sedProgram	*next;
  struct sedCmd		cmd;
};

/* 
** ----------- S command -----------
** concatenate recipe to create the replacement:
** recipe values of 0 to 9 are backreferences,
** (char *) -1 ends the recipe,
** and other values point somewhere in 'text'.
** Note: the lowest and highest pointer values should never
** overlap malloc()'d memory,
** they are reserved for the kernel or special meaning
*/
struct SReplacement		{
  char			*text; //private reference string
  size_t 		n_refs;
  char			**recipe;
};

struct			SCmd	{
  regex_t		pattern;
  struct SReplacement	new;
  // - options -
  unsigned		g:	1;
  unsigned		p:	1;
  unsigned		e:	1;
  unsigned		d:	1; // if 'p' given before 'e' 
  FILE			*w;
  int			number;
 // 'i' and 'm' affect regcomp() 
};

/*
**  ----------- Function Prototypes  -----------
*/

void	*xmalloc(size_t len);
void	*xrealloc(void *ptr, size_t len);
FILE	*xfopen(const char *f_name, const char *mode);

struct vbuf	*vbuf_new();
ssize_t		vbuf_getline(struct vbuf *text, FILE *in);
struct vbuf	*read_text();
struct vbuf	*snarf(char delim);
struct vbuf	*vbuf_readName();
char		*vbuf_tostring(struct vbuf *);

struct sedLine	*sedLine_new();
void		sedLine_appendText
  (struct sedLine *l, char *text, int len);
void		sedLine_deleteEmbeddedLine
  (struct sedLine *l, FILE *out);
void		sedLine_appendLineList
  (struct sedLine *line, struct sedLine *ap);
void		sedLine_deleteLine
  (struct sedLine *l, FILE *out);

struct sedProgram	*compile_file
  (struct sedProgram *compile, const char *f_name);
struct sedProgram	*compile_string
  (struct sedProgram *compile, char *str);
char	exec_stream
  (struct sedProgram *prog, char **files);

#endif // ifndef DATA_H_
