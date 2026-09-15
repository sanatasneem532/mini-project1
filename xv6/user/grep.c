

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char buf[1024];
int m(char *, char *);

void g(char *pattern, int fd)
{
  int a, b;
  char *x, *y;

  b = 0;
  while ((a = read(fd, buf + b, sizeof(buf) - b - 1)) > 0) {
    b += a;
    buf[b] = '\0';
    x = buf;
    while ((y = strchr(x, '\n')) != 0) {
      *y = 0;
      if (m(pattern, x)) {
        *y = '\n';
        write(1, x, y + 1 - x);
      }
      x = y + 1;
    }
    if (b > 0) {
      b -= x - buf;
      memmove(buf, x, b);
    }
  }
}

int
main(int argc, char *argv[])
{
  int fd, i;
  char *pattern;

  if (argc <= 1) {
    fprintf(2, "usage: grep pattern [file ...]\n");
    exit(1);
  }
  pattern = argv[1];

  if (argc <= 2) {
    g(pattern, 0);
    exit(0);
  }

  for (i = 2; i < argc; i++) {
    if ((fd = open(argv[i], O_RDONLY)) < 0) {
      printf("grep: cannot open %s\n", argv[i]);
      exit(1);
    }
    g(pattern, fd);
    close(fd);
  }
  exit(0);
}



int h(char *, char *);
int s(int, char *, char *);

int m(char *re, char *text)
{
  if (re[0] == '^')
    return h(re + 1, text);
  do {
    if (h(re, text))
      return 1;
  } while (*text++ != '\0');
  return 0;
}

// matchhere: search for re at beginning of text
int h(char *re, char *text)
{
  if (re[0] == '\0')
    return 1;
  if (re[1] == '*')
    return s(re[0], re + 2, text);
  if (re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if (*text != '\0' && (re[0] == '.' || re[0] == *text))
    return h(re + 1, text + 1);
  return 0;
}

// matchstar: search for c*re at beginning of text
int s(int c, char *re, char *text)
{
  do {
    if (h(re, text))
      return 1;
  } while (*text != '\0' && (*text++ == c || c == '.'));
  return 0;
}
