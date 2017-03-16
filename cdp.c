#include <stdio.h>
#include <stdlib.h>
#include "ext.h"
#include "ext_obex.h"
#include "ext_systhread.h"
#include "ext_buffer.h"
#include "cdp.h"
#include "cdptask.h"
#include "process_osx.h"

#define OVERWRITE_ENABLED  (1)
#define OVERWRITE_DISABLED (0)
#define OVERWRITE_DEFAULT  OVERWRITE_ENABLED


void cdp_anything(t_cdp *x, t_symbol *s, long ac, t_atom *av);
void cdp_docdp(t_cdp *x, t_symbol *s, long ac, t_atom *av);

void cdp_listprograms(t_cdp *x);
void cdp_taskcomplete(t_cdp *x, t_symbol *s, long ac, t_atom *av);
void cdp_taskoutput(t_cdp *x, t_symbol *s, long ac, t_atom *av);
void cdp_cancel(t_cdp *x);
void cdp_stop(t_cdp *x);
void cdp_assist(t_cdp *x, void *b, long m, long a, char *s);
void cdp_free(t_cdp *x);
void *cdp_new(t_symbol *s, long ac, t_atom *av);

t_max_err cdp_find_excutable(t_cdp *x, char *name, short *path, char *full_path);
t_max_err cdp_make_cmd(t_cdp *x, char *executable, long ac, t_atom *av, t_string **cmd);
void cdp_do_program(t_cdp *x, char *program, long ac, t_atom *av);

t_max_err cdp_substitute_buffers(t_cdp *x, long ac, t_atom *av, t_atom *av_sub, long *subbed);
t_max_err cdp_reload_output_buffers(t_cdp *x, long ac, t_atom *av, t_atom *av_sub, long *subbed);


char *get_filename_ext(char *filename);
bool array_any(long *arr, long n);
bool is_cdp_input_buffer(t_symbol *s);
bool is_cdp_output_buffer(t_symbol *s);
int write_tmp_wav(t_buffer_ref *buf, t_symbol *filename);
int delete_existing_file(t_symbol *filename);
int delete_temporary_wavs(long ac, t_atom *av, long *subbed);

t_class *cdp_class;


void ext_main(void *r)
{
  t_class *c;

  c = class_new("cdp", (method)cdp_new, (method)cdp_free, sizeof(t_cdp), 0L, A_GIMME, 0);

  class_addmethod(c, (method)cdp_cancel,       "cancel",       0);
  class_addmethod(c, (method)cdp_listprograms, "listprograms", 0);
  class_addmethod(c, (method)cdp_anything,     "anything",     A_GIMME, 0);
  
  // methods which we use internally. they all have A_GIMME style signature
  class_addmethod(c, (method)cdp_docdp,        "docpd",        A_GIMME, 0);
  class_addmethod(c, (method)cdp_taskcomplete, "taskcomplete", A_GIMME, 0);
  class_addmethod(c, (method)cdp_taskoutput,   "taskoutput",   A_GIMME, 0);

  class_addmethod(c, (method)cdp_assist,       "assist",       A_CANT, 0);
  
  // Attributes
  CLASS_ATTR_SYM(c, "root", 0, t_cdp, cdp_path);
  CLASS_ATTR_LABEL(c, "root", 0, "CDP directory");

  CLASS_ATTR_LONG(c, "overwrite", 0, t_cdp, overwrite);

  class_register(CLASS_BOX,c);
  cdp_class = c;

  cdptask_init(); // initialze one global background task thread pool for all of our objects
}


/*
 There are roughly 1 bajjilion CDP programs. It's not worth creating methods for each
 one so instead we assume that any undefined message is a CDP command and try to
 execute it.
 
 Allocate a new set of atoms comprising the CDP program name and arguments.
 */
void cdp_anything(t_cdp *x, t_symbol *s, long ac, t_atom *av)
{
  long ac_command = 0;
  t_atom *av_command = NULL;
  char alloc;
  
  if (atom_alloc_array(ac + 1, &ac_command, &av_command, &alloc) == MAX_ERR_NONE) {
    atom_setsym(av_command, s);
    sysmem_copyptr(av, av_command + 1, sizeof(t_atom) * ac);
    
    cdptask_execute_method((t_object *)x,gensym("docpd"),ac_command,av_command,
                           (t_object *)x,gensym("taskcomplete"),ac_command,av_command,
                           NULL,0);
  }

  if (av_command)
    sysmem_freeptr(av_command);
}


/*
 Called from the threadpool
 av contains at least the name of the CPD program, plus optional arguments
 */
void cdp_docdp(t_cdp *x, t_symbol *s, long ac, t_atom *av)
{
  char *program = NULL;
  char *text = NULL;
  long textsize = 0;
  long num_args = ac - 1;
  t_atom *args = ac > 1 ? av + 1 : NULL;
  
  
  if (av) {
    program = atom_getsym(av)->s_name;
    if (args) {
      atom_gettext(ac-1, args, &textsize, &text, 0);
    }
    
    post("cdp_docdp ac:%d program:%s args:%s", ac, program, text?text:"");
    
    if (text)
      sysmem_freeptr(text);
    
    cdp_do_program(x, program, num_args, args);
  }
}


void cdp_listprograms(t_cdp *x)
{
  char full_path[MAX_PATH_CHARS];
  char filename[MAX_FILENAME_CHARS];
  short path_id = 0;
  void *dir;
  t_fourcc filetype;
  t_atom a;
  t_atomarray *aa;
  long ac;
  t_atom *av;
  
  aa = atomarray_new(0, NULL);

  if (aa && !cdp_find_excutable(x, "filter", &path_id, full_path)) {
    dir = path_openfolder(path_id);
    while(path_foldernextfile(dir, &filetype, filename, 0)) {
      if (strcmp(get_filename_ext(filename), "sh")) // ignore shell scripts
      {
        atom_setsym(&a, gensym(filename));
        atomarray_appendatom(aa, &a);
      }
    }
    path_closefolder(dir);
    atomarray_getatoms(aa, &ac, &av);
    defer_low(x, (method)cdp_taskoutput, gensym("listprograms"), ac, av);
    object_free(aa);
  } else {
    object_error((t_object*)x, "Could not locate CDP programs");
  }
}


void cdp_taskcomplete(t_cdp *x, t_symbol *s, long ac, t_atom *av)
{
  long textsize=0;
  char *tmpstr=NULL;
  // our completion method will be called from our bakground thread
  // we cannot output to a patcher from this thread (ILLEGAL)
  // so we must defer or schedule output to the patcher

  atom_gettext(ac,av,&textsize,&tmpstr,0);
  post("cdp background task (%s) completed in thread %x",tmpstr,systhread_self());

  defer_low(x, (method)cdp_taskoutput, gensym("taskoutput"), ac, av);

  if (tmpstr)
    sysmem_freeptr(tmpstr);
}

void cdp_taskoutput(t_cdp *x, t_symbol *s, long ac, t_atom *av)
{
  outlet_anything(x->x_outlet, s, ac, av);
}

void cdp_cancel(t_cdp *x)
{
  cdp_stop(x);
  outlet_anything(x->x_outlet,gensym("cancelled"), 0, NULL);
}

void cdp_stop(t_cdp *x)
{
  // stop all tasks associated with my object if they are still present
  cdptask_purge_object((t_object *)x);
}

void cdp_assist(t_cdp *x, void *b, long m, long a, char *s)
{
  if (m==1)
    sprintf(s,"make a new task");
  else if (m==2)
    sprintf(s,"report when done/cancelled");
}

void cdp_free(t_cdp *x)
{
  cdp_stop(x);
}

void *cdp_new(t_symbol *s, long ac, t_atom *av)
{
  t_cdp *x;

  x = (t_cdp *)object_alloc(cdp_class);
  x->x_outlet = outlet_new(x,NULL);
  x->cdp_path = gensym("");
  x->overwrite = OVERWRITE_DEFAULT;

  attr_args_process(x, ac, av);
  
  return(x);
}



///////////////////////////////////////////
//  Helper methods to run a CDP process  //
///////////////////////////////////////////

t_max_err cdp_find_excutable(t_cdp *x, char *name, short *path, char *full_path)
{
  char tmp[MAX_PATH_CHARS];
  t_fourcc type = 0;
  t_max_err err = 0;

  strcpy(tmp, x->cdp_path->s_name);
  
  // TODO: Simplify this logic
  
  // If user has specifed a location, look for it and use it if it exists.
  // Else, look in the Max search path.
  if (x->cdp_path && x->cdp_path != gensym("") && !locatefile_extended(tmp, path, &type, NULL, 0)) {
    // User has defined a CDP root and it exists. Now check for executable.
    err = path_topathname(*path, name, full_path);
  } else {
    // No CPD root defined. Check Max's search path.
    err = locatefile_extended(name, path, &type, NULL, 0);
    if (!err) {
      err = path_topathname(*path, name, full_path);
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
  t_atom av_out[2];
  
  short path = 0;
  char fullpath[MAX_PATH_CHARS];
  
  // For substituting buffer~s
  t_atom *av_sub = NULL;
  long ac_sub = 0;
  char alloc = 0;
  long *subbed = NULL;
  
  
  memset(process_output, 0, sizeof(process_output));
  
  if (cdp_find_excutable(x, program, &path, fullpath)) {
    object_error((t_object*)x, "could not find excutable %s", program);
    goto end;
  } else {
    post("found excutable %d %s", path, fullpath);
  }
  
  if (ac > 0) {
    if (atom_alloc_array(ac, &ac_sub, &av_sub, &alloc) != MAX_ERR_NONE)
      goto end;
    
    subbed = (long *)sysmem_newptr(ac * sizeof(long));
    if (!subbed)
      goto end;
    
    if (cdp_substitute_buffers(x, ac, av, av_sub, subbed) != MAX_ERR_NONE) {
      goto end;
    }
    
    if (cdp_make_cmd(x, fullpath, ac, av_sub, &cmd)) {
      goto end;
    }
  } else {
    if (cdp_make_cmd(x, fullpath, ac, av, &cmd)) {
      goto end;
    }
  }
  
  
  // Run the program here
  process_return = run_process((char*)string_getptr(cmd), process_output, PROCESS_OUTPUT_MAX_SIZE);
  atom_setlong(av_out, process_return);
  atom_setsym(av_out+1, gensym(process_output));
  defer_low(x, (method)cdp_taskoutput, gensym("cdpout"), 2, av_out);
  
  if (ac > 0) {
    cdp_reload_output_buffers(x, ac, av, av_sub, subbed);
  }
  
end:
  // FIXME: Files get deleted before the buffer can reload
//  if (av_sub && subbed)
//    delete_temporary_wavs(ac, av_sub, subbed);
  if (av_sub)
    sysmem_freeptr(av_sub);
  if (subbed)
    sysmem_freeptr(subbed);
  if (cmd)
    object_free(cmd);
}


// Find CDP program arguments that are names of Max buffer~ objects and substitute them for temporary wav files.
// Loop through ac atoms in av, editing the corresponding atom in av_sub if there's a buffer with that name.
// For each edit, set the corresponding value in subbed to TRUE.
t_max_err cdp_substitute_buffers(t_cdp *x, long ac, t_atom *av, t_atom *av_sub, long *subbed)
{
  t_buffer_ref *ref = NULL;
  t_symbol *s = NULL;
  char tmpfilename[MAX_FILENAME_CHARS];
  int i;
  
  if (ac < 1)
    return MAX_ERR_NONE;

  sysmem_copyptr(av, av_sub, sizeof(t_atom) * ac);
  memset(subbed, 0, ac * sizeof(long));
  
  for(i = 0; i < ac; i++) {
    if (atom_gettype(&av[i]) == A_SYM) {
      s = atom_getsym(&av[i]);
      ref = buffer_ref_new((t_object *)x, s);
      if (ref) {
        if (buffer_ref_exists(ref)) {
          get_tmp_file_name(tmpfilename, sizeof(tmpfilename), s->s_name);
          atom_setsym(&av_sub[i], gensym(tmpfilename));
          subbed[i] = 1;
          
          if (is_cdp_input_buffer(s)) {
            if(write_tmp_wav(ref, gensym(tmpfilename))) return MAX_ERR_GENERIC;
          } else if(is_cdp_output_buffer(s)) {
            if(delete_existing_file(gensym(tmpfilename))) return MAX_ERR_GENERIC;
          }
        }
        object_free(ref);
      }
    }
  }

  return MAX_ERR_NONE;
}



static void load_file_into_buffer(t_object *x, t_symbol *filename, t_symbol *buffername)
{
  t_buffer_ref *bufferref = NULL;
  t_buffer_obj *buffer = NULL;
  t_atom a_ret;
  
  if ((bufferref = buffer_ref_new(x, buffername))) {
    if ((buffer = buffer_ref_getobject(bufferref))) {
      object_method_sym(buffer, gensym("replace"), filename, &a_ret);
    }
    object_free(bufferref);
  }
}

// After processing, load output wavs into their corresponding buffers
t_max_err cdp_reload_output_buffers(t_cdp *x, long ac, t_atom *av, t_atom *av_sub, long *subbed)
{
  
  t_symbol *filename = NULL, *buffername = NULL;
  int i;
  
  for (i = 0; i < ac; i++) {
    if (subbed[i]) {
      filename = atom_getsym(&av_sub[i]);
      buffername = atom_getsym(&av[i]);
      if (is_cdp_output_buffer(buffername)) {
        load_file_into_buffer((t_object*)x, filename, buffername);
      }
    }
  }
  
  return MAX_ERR_NONE;
}


char *get_filename_ext(char *filename)
{
  char *dot = strrchr(filename, '.');
  if(!dot || dot == filename) return "";
  return dot + 1;
}


bool array_any(long *arr, long n)
{
  long result = 0, i = n;
  while(--i) {
    result = result || arr[i];
  }
  return result;
}


bool is_cdp_input_buffer(t_symbol *s)
{
  return !memcmp(s->s_name, CDP_INPUT_PREFIX, CDP_INPUT_PREFIX_SIZE);
}

bool is_cdp_output_buffer(t_symbol *s)
{
  return !memcmp(s->s_name, CDP_OUTPUT_PREFIX, CDP_OUTPUT_PREFIX_SIZE);
}


int write_tmp_wav(t_buffer_ref *ref, t_symbol *filename)
{
  t_buffer_obj *buffer = NULL;
  t_atom a_ret;
  
  buffer = buffer_ref_getobject(ref);
  object_method_sym(buffer, gensym("writewave"), filename, &a_ret);
  
  return 0;
}


int delete_existing_file(t_symbol *filename)
{
  int ret = 0;
  
  ret = unlink(filename->s_name);
  if (ret && errno != ENOENT) { // Ignore "file doesn't exist" error
    error("Cannot delete file %s, error %d \"%s\"", filename->s_name, errno, strerror(errno));
    return errno;
  }
  
  return 0;
}


int delete_temporary_wavs(long ac, t_atom *av, long *subbed)
{
  int i, ret=0;
  
  for (i = 0; i < ac; i++) {
    if (subbed[i] && (ret = delete_existing_file(atom_getsym(&av[i])))) {
      return ret;
    }
  }
  
  return 0;
}













