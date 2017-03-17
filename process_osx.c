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
  char tmp[MAX_PATH_CHARS+5];
  char suffix[5] = {'.','w','a','v', NULL};
  char *end;

  if(path_toabsolutesystempath(path_getdefault(), prefix, tmp) != MAX_ERR_NONE) {
    return -1;
  }
  end = strchr(tmp, NULL);
  sysmem_copyptr(suffix, end, 5);
  
  return path_nameconform(tmp, buf, PATH_STYLE_NATIVE, PATH_TYPE_BOOT);
}
