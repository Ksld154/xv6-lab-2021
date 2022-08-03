#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  
  char* str = "Hello!\n";
  write(1, str, strlen(str));
  
  exit(0);
}
