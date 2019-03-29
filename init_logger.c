#include <stdio.h>

int main(int argc, char const* argv[]) {
  printf("init_logger\n");
  fflush(stdout);
  FILE *fp = fopen("/tmp/a", "rw");
  if( fp == NULL ) {
    fprintf(stderr, "erro file open\n");
    return 1;
  }

  char readline[1024] = {'\0'};
  while (fgets(readline, 1024, stdin)) {
    fprintf(fp, "%s", readline);
    fflush(fp);
  }
}
