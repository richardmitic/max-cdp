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


int get_tmp_file_name(char *buf, int buflen, char *prefix)
{
  char tmp[MAX_PATH_CHARS];
  
  snprintf_zero(tmp, buflen, "~/%s.wav", prefix);
  return path_nameconform(tmp, buf, PATH_STYLE_NATIVE, PATH_TYPE_BOOT);
}
