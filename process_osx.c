//
//  process.c
//  cdp
//
//  Created by Richard Mitic on 27/02/2017.
//
//

#include <stdio.h>
#include "ext.h"
#include "process_osx.h"


int run_process(char *cmd, char *stdout, int stdout_size)
{
  FILE *process = NULL;
  
  process = popen(cmd, "r");
  if (!process) {
    return -1;
  }
  
  fread(stdout, sizeof(char), stdout_size, process);
  return pclose(process);
}
