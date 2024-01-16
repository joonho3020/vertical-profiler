#include <stdio.h>

#ifdef LINUX
#include <unistd.h>
#endif

int main() {
  for (int i = 0; i < 10; i++)
    printf("hello world %d\n", i);

#ifdef LINUX
  printf("pid: %ld ppid: %ld", (long)getpid(), (long)getppid());
#endif
  return 0;
}
