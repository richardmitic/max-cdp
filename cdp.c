#include <stdio.h>
#include "ext.h"
#include "ext_obex.h"
#include "ext_systhread.h"
#include "cdp.h"
#include "cdptask.h"
#include "cdpprogram.h"
#include "cdp_methods.h"
#include "cdp_domethods.h"
#include "class_methods.h"
#include "class_domethods.h"


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



t_class *cdp_class;


void ext_main(void *r)
{
	t_class *c;

	c = class_new("cdp", (method)cdp_new, (method)cdp_free, sizeof(t_cdp), 0L, A_GIMME, 0);

	class_addmethod(c, (method)cdp_cancel,       "cancel",       0);
  class_addmethod(c, (method)cdp_listprograms, "listprograms", 0);
  class_addmethod(c, (method)cdp_anything,     "anything",     A_GIMME, 0);
  
  
  // TODO: Adding class methods with a macro seems pretty sketchy. Find a better way.
//  CLASS_ADD_CDP_METHODS(c);
  
	// methods which we use internally. they all have A_GIMME style signature
//  CLASS_ADD_CDP_DO_METHODS(c);
  
  class_addmethod(c, (method)cdp_docdp,    "docpd",    A_GIMME, 0);
  
	class_addmethod(c, (method)cdp_taskcomplete, "taskcomplete", A_GIMME, 0);
	class_addmethod(c, (method)cdp_taskoutput,   "taskoutput",   A_GIMME, 0);

	class_addmethod(c, (method)cdp_assist,       "assist",       A_CANT, 0);
  
  // Attributes
  CLASS_ATTR_SYM(c, "root", 0, t_cdp, cdp_path);
  CLASS_ATTR_LABEL(c, "root", 0, "CDP directory");

	class_register(CLASS_BOX,c);
	cdp_class = c;

	cdptask_init(); // initialze one global background task thread pool for all of our objects
}


/*
 There are roughly 1 bajjilion CDP programs. It's not worth creating methods for each
 one so instead we assume that any undefined message is a CDP command and try to
 execute it.
 
 Allocate a new set of atoms comprising the CDP program name and arguments.
 The allocated memory will be used in another thread and must be freed in taskcomplete.
 */
void cdp_anything(t_cdp *x, t_symbol *s, long ac, t_atom *av)
{
  long ac_command = 0;
  t_atom *av_command = NULL;
  char alloc;
  
  if (atom_alloc_array(ac + 1, &ac_command, &av_command, &alloc) == MAX_ERR_NONE) {
    x->allocated_memory += sysmem_ptrsize(av_command);
    atom_setsym(av_command, s);
    sysmem_copyptr(av, av_command + 1, sizeof(t_atom) * ac);
    
    cdptask_execute_method((t_object *)x,gensym("docpd"),ac_command,av_command,
                           (t_object *)x,gensym("taskcomplete"),ac_command,av_command,
                           NULL,0);
  }
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
  
  if (!cdp_find_excutable(x, "filter", &path_id, full_path)) {
    dir = path_openfolder(path_id);
    while(path_foldernextfile(dir, &filetype, filename, 0)) {
      if (strcmp(get_filename_ext(filename), "sh")) // ignore shell scripts
        post("%s", filename);
    }
    path_closefolder(dir);
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
	//schedule_delay(x,(method)cdp_taskoutput,gensym("taskoutput"),ac,av);

	if (tmpstr)
		sysmem_freeptr(tmpstr);
  
  // We allocated this memory in cdp_anything. Free it now we're definitely finished.
  if (av)
    sysmem_freeptr(av);
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
  
  attr_args_process(x, ac, av);
  
	return(x);
}






