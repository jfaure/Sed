#include <iostream>
#include <string>
#include <stdlib.h>
#include <sys/time.h>

#define randnum rand() % 10 + '0'
#define randop "1-%/*"[rand() % 5]

std::string	genexpr(int len)
{
  static int parens = 0;
  std::string r;
  char	c;

  while (len--)
  {
    c = rand() % 2 && r.back() != '-' ? '-' : randnum;
    r.push_back(c);
    if (c == '-') r.push_back(randnum);
    if (rand() % 3 == 0 && parens > 0)
      r.push_back(')'), --parens;
    r.push_back(randop);
    if (rand() % 3 == 0)
      r.push_back('('), ++parens;
  }
  r.push_back(randnum);
  while (parens-- > 0)
    r.push_back(')');
  return (r);
}

int	main(int ac, char **av)
{
  struct timeval t;

  gettimeofday(&t, 0);
  srand(t.tv_usec);
  ac > 1 && (ac = atoi(av[1]));
  std::cout << genexpr(ac) << std::endl;
}
