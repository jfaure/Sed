#include "data.h"

struct sedOptions	g_sedOptions = {0, 0, 0, 0, 0, 0};

void	usage(int exit_status)
{
  //execl("/usr/bin/sed", "/usr/bin/sed", 0);
  fputs("Usage\n", stdout);
  exit(exit_status);
}

void	panic(const char *why, ...)
{
  va_list	ap;

  va_start(ap, why); vfprintf(stderr, why, ap); va_end(ap); putc(10, stderr);
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
  printf("xrealloc(len = %d)\n", len);
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

  while ((opt = getopt_long(ac, av, shortopts, longopts, NULL)) != -1)
    switch (opt)
    {
      case 'i': g_sedOptions.in_place = 1; // -i implies -s
      case 's': g_sedOptions.separate = 1; break;
      case 'n': g_sedOptions.silent = 1; break;
      case 'f': prog_addScript("", optarg); break;
      case 'e': prog_addScript(optarg, NULL); break;
      default:  usage(4);
    }
  if (!g_in.next)
    if (optind < ac)
      prog_addScript(av[optind++], NULL);
    else
      usage(1);
  compile_program(&prog);
  fputs("-----------------------------\n", stdout);
  return (exec_stream(&prog, av + optind));
}
