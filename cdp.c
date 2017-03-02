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
  
  // TODO: Adding class methods with a macro seems pretty sketchy. Find a better way.
  CLASS_ADD_CDP_METHODS(c);
  
	// methods which we use internally. they all have A_GIMME style signature
  CLASS_ADD_CDP_DO_METHODS(c);
  
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


void cdp_listprograms(t_cdp *x)
{
  
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
	post("deferring output to the main thread");

	defer_low(x,(method)cdp_taskoutput,gensym("taskoutput"),ac,av);
	//schedule_delay(x,(method)cdp_taskoutput,gensym("taskoutput"),ac,av);

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
  
  attr_args_process(x, ac, av);
  
	return(x);
}







