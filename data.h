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

struct		sedOptions	{
  unsigned	silent:			1;
  unsigned	follow_symlinks:	1;
  unsigned	in_place:		1;
  unsigned	null_data:		1;
  unsigned	separate:		1; // reset line count for each file
  unsigned	extended_regex_syntax:	1;
};

char asdf;
extern struct sedOptions g_sedOptions;

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

#define nextChar() ((asdf = *g_in.cursor == 0 ? nextProgStream() : *g_in.cursor++) \
    + !printf("nextChar %c\n", asdf))
#define prevChar(c) assert(asdf = g_in.cursor >= g_in.next->buf ? *g_in.cursor : g_in.last\
    + !printf("prevChar %c\n", asdf))

struct		lstring	{ // Interface for automatic string memory managment
  char		*buf;
  size_t	len;
  size_t	alloc;
};

/*
** Data for pattern and holdspace. (encapsulation for lstring)
** We always maintain at least one line of lookahead (nextLine),
** to tell if $ address matches active.
*/
struct			lineList	{  // Data for fast line deletion and append.
  char			*buf;
  size_t		alloc;
  char			*active;
  size_t		len;
  struct lstring	*lookahead;
};

struct sedRegex	{
  regex_t	compile;
  int		flags;
};

enum sedAddrType	{
  ADDR_NONE, // for cmdaddresses like '1,' and ',9'
  ADDR_LINE,
  ADDR_END, // '$'
  ADDR_REGEX,
};

struct			sedAddr	{
  enum sedAddrType	type; // basically regex or line_no
  union	{
    int			line;
    struct sedRegex	regex;
  }			info;
};

enum sedCmdAddrType	{ // struct sedCmdAddr: do we use a1, a2, step ?
  CMD_ADDR_DONE,  // address will never match again
  CMD_ADDR_LINE, // use only a1
  CMD_ADDR_RANGE, // use a1, a2 
  CMD_ADDR_STEP, // use a1, a2, step
};

struct			sedCmdAddr	{ // this must handle one or two sedAddr, steps (~) and non_matches ('!').
  unsigned		bang:	1; 	// cmd applies to non-matches
  enum sedCmdAddrType	type;
  struct sedAddr	a1, a2;
  size_t		step;
};

struct			sedLabel	{
  char			*name;
  int			pos;
  struct sedLabel	*next;
};

struct 			sedCmd	{
  struct sedCmdAddr	*addr; // NULL to match unconditionally
  char			cmdChar;
  union		{
    struct SCmd		*s;
    char		*y;
    struct lstring	*text; // For a, i commands
    int			int_arg;
    FILE		*file;
  }			info;
};

struct			sedProgram	{ // List of struct sedCmds
  struct sedCmd		*cmdStack;
  int			len;
  int			pos;
};

struct 			sedInput	{ // Used for reading in prog
  char			*str;		// If null, read from file. both must not be null.
  FILE			*file;
  int			line;		// For meaningful error messages
}			g_progInput;

struct subString	{
  int	 		so;
  int			len;
};

struct SReplacement		{
  char			*text;
  size_t 		n_refs;
  struct subString	*ref;
};

struct			SCmd	{
  struct sedRegex	pattern;
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

struct lstring	*lstring_new();
ssize_t	lstring_getline(struct lstring *text, FILE *in);
struct lstring	*read_text();
struct lstring	*snarf(char delim);

struct lineList	*lineList_new();
void		lineList_appendText(struct lineList *l, char *text, int len);
void		lineList_deleteEmbeddedLine(struct lineList *l, FILE *out);
void		lineList_appendLineList(struct lineList *line, struct lineList *ap);
void		lineList_deleteLine(struct lineList *l, FILE *out);

struct sedProgram	*compile_file(struct sedProgram *compile, const char *f_name);
struct sedProgram	*compile_string(struct sedProgram *compile, char *str);
char	exec_stream(struct sedProgram *prog, int ac, char **files);

#endif // ifndef DATA_H_
