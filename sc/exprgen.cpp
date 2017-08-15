#include <iostream>
#include <string>
#include <stdlib.h>
#include <sys/time.h>

#define randnum (rand() % 10 + '0')
#define randop "+-%/*"[rand() % 5]

std::string	genexpr(int len)
{
  static int parens = 0;
  std::string r;
  char	c;

  while (len--)
  {
    r += c = rand() % 2 && c != '-' ? '-' : randnum;
    if (c == '-')
      r += randnum;
    if (rand() % 3 == 0 && parens > 0)
      r += ')', --parens;
    r += randop;
    if (rand() % 3 == 0)
      r += '(', ++parens;
  }
  return (r + (char) randnum + std::string(parens, ')'));
}

int	main(int ac, char **av)
{
  struct timeval t;

  gettimeofday(&t, 0);
  srand(t.tv_usec);
  ac > 1 && (ac = atoi(av[1]));
  std::cout << genexpr(ac) << std::endl;
}
