#include "data.h"

char const	*progname;

void	panic(const char *why, ...)
{
  va_list	ap;

  fprintf(stderr, "%s (line %d): ", progname, g_in.line);
  va_start(ap, why); vfprintf(stderr, why, ap); va_end(ap);
  putc(10, stderr);
  exit(4);
}

void	*xmalloc(size_t len)
{
  void	*r;

  if (r = malloc(len))
    return (r);
  panic("malloc returned NULL");
}

void	*xrealloc(void *this, size_t len)
{
  if (this = realloc(this, len))
    return (this);
  panic("realloc returned NULL");
}

FILE	*xfopen(const char *f_name, const char *mode)
{
  FILE	*r;

  if (r = fopen(f_name, mode))
    return (r);
  panic("fopen returned NULL");
}

int	main(int ac, char **av)
{
  char const *const	shortopts = "si::nf:e:";
  struct option		longopts[] = {
    {"in-place", 2, 0, 'i'},
    {"separate", 0, 0, 's'},
    {"file", 1, 0, 'f'},
    {"silent", 0, 0, 'n'}, {"quiet", 0, 0, 'n'},
    {NULL, 0, NULL, 0}
  };
  char			opt;
  struct sedProgram	prog;
  extern struct sedOptions	g_sedOptions;
  extern struct sedRuntime	g_lineInfo;

  progname = *av;
  memset(&g_sedOptions, 0, sizeof(g_sedOptions));
  memset(&g_lineInfo,   0, sizeof(g_lineInfo));
  while ((opt = getopt_long(ac, av, shortopts, longopts, NULL)) != -1)
    switch (opt) {
    case 'i': g_sedOptions.in_place = 1; // -i implies -s
    case 's': g_sedOptions.separate = 1; break;
    case 'n': g_sedOptions.silent = 1; break;
    case 'f': prog_addScript("", optarg); break;
    case 'e': prog_addScript(optarg, NULL); break;
    default:  return (1);
    }
  if (!g_in.info) // no -e or -f scripts
    if (optind < ac)
      prog_addScript(av[optind++], NULL);
    else
      panic("%s requires a script.", progname);
  compile_program(&prog);
  opt = exec_stream(&prog, av + optind);
  compile_program(NULL);
  return (opt);
}
