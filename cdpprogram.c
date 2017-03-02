//
//  cdpprogram.c
//  cdp
//
//  Created by Richard Mitic on 28/02/2017.
//
//

#include <string.h>
#include "cdpprogram.h"
#include "ext.h"
#include "ext_obex.h"
#include "cdp.h"
#include "process_osx.h"

/*
  Util functions
 */
t_max_err cdp_find_excutable(t_cdp *x, char *name, short *path, char *full_path)
{
  char tmp[MAX_PATH_CHARS];
  t_fourcc type = 0;
  t_max_err err = 0;
  
  strcpy(tmp, x->cdp_path->s_name);
  
  // If user has specifed a location, look for it and use it if it exists.
  // Else, look in the Max search path.
  if (x->cdp_path && x->cdp_path != gensym("") && !locatefile_extended(tmp, path, &type, NULL, 0)) {
    err = path_toabsolutesystempath(*path, name, full_path);
  } else {
    err = locatefile_extended(name, path, &type, NULL, 0);
    if (!err) {
      err = path_toabsolutesystempath(path, name, full_path);
    }
  }
  
  if (!err) {
    strcpy(tmp, full_path);
    path_nameconform(tmp, full_path, PATH_STYLE_NATIVE, PATH_TYPE_BOOT);
  }
  
  return err;
}


t_max_err cdp_make_cmd(t_cdp *x, char *executable, long ac, t_atom *av, t_string **cmd)
{
  long argstrsize = 0;
  char *argstr = NULL;
  t_string *cmdstr = NULL;
  
  cmdstr = string_new(executable);
  if (!cmdstr) {
    object_error((t_object*)x, "could not create command string for %s", executable);
    return 1;
  }
  
  string_append(cmdstr, " ");
  
  atom_gettext(ac, av, &argstrsize, &argstr, 0);
  if (!argstr) {
    object_error((t_object*)x, "could not get arguments for %s", string_getptr(cmdstr));
    return 2;
  }
  
  string_append(cmdstr, argstr);
  
  *cmd = cmdstr;
  
  if (argstr)
    sysmem_freeptr(argstr);
  
  return 0;
}


void cdp_do_program(t_cdp *x, char *program, long ac, t_atom *av)
{
  t_string *cmd = NULL;
  char process_output[PROCESS_OUTPUT_MAX_SIZE];
  int process_return;
  
  short path = 0;
  char fullpath[MAX_PATH_CHARS];
  
  if (cdp_find_excutable(x, program, &path, fullpath)) {
    object_error((t_object*)x, "could not find excutable %s", program);
    return;
  } else {
    post("found excutable %d %s", path, fullpath);
  }
  
  if (cdp_make_cmd(x, fullpath, ac, av, &cmd)) {
    return;
  }
  
  process_return = run_process((char*)string_getptr(cmd), process_output, PROCESS_OUTPUT_MAX_SIZE);
  
  post("cdp: \"%s\" return %d output \"%s\" ", string_getptr(cmd), process_return, process_output);
  
  if (cmd)
    object_free(cmd);
}


char *get_filename_ext(char *filename)
{
  char *dot = strrchr(filename, '.');
  if(!dot || dot == filename) return "";
  return dot + 1;
}





