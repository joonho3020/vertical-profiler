#include <assert.h>
#include <stdio.h>


int main() {
  printf("assert true\n");
  assert(true);

  printf("assert false\n");
  assert(false);
  printf("assert ignored\n");

  return 0;
}
