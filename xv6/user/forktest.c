

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define N 1000

void p(const char *s)
{
  write(1, s, strlen(s));
}

void f(void)
{
  int a, b;

  p("fork test\n");

  for (a = 0; a < N; a++) {
    b = fork();
    if (b < 0)
      break;
    if (b == 0)
      exit(0);
  }

  if (a == N) {
    p("fork claimed to work N times!\n");
    exit(1);
  }

  for (; a > 0; a--) {
    if (wait(0) < 0) {
      p("wait stopped early\n");
      exit(1);
    }
  }

  if (wait(0) != -1) {
    p("wait got too many\n");
    exit(1);
  }

  p("fork test OK\n");
}

int
main(void)
{
  f();
  exit(0);
}
