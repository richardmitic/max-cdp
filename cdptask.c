
// cdptask.c

#include "ext.h"
#include "ext_obex.h"
#include "ext_systhread.h"
#include "cdptask.h"


// POSSIBLE IMPROVEMENT:
// could reduce latency and polling, if we used cond vars instead of sleeping threads
// a good pthread tutorial is online at http://www.laptev.org/doc/pthreads.html#condvar
// for the most part you, can just replace the use "pthread" with "systhread"


#define cdpTASK_THREAD_COUNT		4
#define cdpTASK_SLEEPTIME		5

// request state
#define cdpTASK_REQ_COMPLETE		0
#define cdpTASK_REQ_PENDING		1
#define cdpTASK_REQ_PROCESSING	2


#define cdpTASK_MUTEX_NEW 	(systhread_mutex_new((t_systhread_mutex *)&s_cdptask_mutex,0))
#define cdpTASK_MUTEX_LOCK 	(systhread_mutex_lock((t_systhread_mutex)s_cdptask_mutex))
#define cdpTASK_MUTEX_UNLOCK 	(systhread_mutex_unlock((t_systhread_mutex)s_cdptask_mutex))

// private
void cdptask_terminate(void);
void cdptask_threadproc(void);
long cdptask_makerequest(t_cdptask *task);
long cdptask_getrequest(t_cdptask **task);
void cdptask_completerequest(t_cdptask *task);

typedef	struct _cdptask_method_caller
{
	t_object	*obtask;
	t_symbol	*mtask;
	long		actask;
	t_atom		*avtask;
	t_object	*obcomp;
	t_symbol	*mcomp;
	long		accomp;
	t_atom		*avcomp;

} t_cdptask_method_caller;

void cdptask_method_caller_task(t_cdptask_method_caller *x);
void cdptask_method_caller_complete(t_cdptask_method_caller *x);
void cdptask_method_caller_free(t_cdptask_method_caller *x);

static t_systhread			s_cdptask_thread_pool[cdpTASK_THREAD_COUNT];
static t_systhread_mutex	s_cdptask_mutex = NULL;
static t_linklist			*s_cdptask_requestlist = NULL;
static t_linklist			*s_cdptask_executelist = NULL;
static long					s_cdptask_numrequests = 0;
static long					s_cdptask_init = 0;
static long					s_cdptask_exit = 0;
static long					s_cdptask_id = 0;


void cdptask_init(void)
{
	long i,err;

	if (s_cdptask_init)
		return;

	cdpTASK_MUTEX_NEW;
	s_cdptask_numrequests = 0;
	s_cdptask_init = 1;
	s_cdptask_requestlist = linklist_new();
	s_cdptask_executelist = linklist_new();

	for (i=0; i<cdpTASK_THREAD_COUNT; i++) {
		if ((err=systhread_create((method)cdptask_threadproc,NULL,0,0,0,s_cdptask_thread_pool+i))) {
			error("cdptask thread could not be created: %d", err);
			s_cdptask_thread_pool[i] = NULL;
		}
	}
	quittask_install((method)cdptask_terminate,NULL);
}

void cdptask_terminate(void)
{
	long i;

	s_cdptask_exit = 1;
	for (i=0; i<cdpTASK_THREAD_COUNT; i++) {
		if (s_cdptask_thread_pool[i]) {
			// may wish to join instead?
			systhread_terminate(s_cdptask_thread_pool[i]);
			s_cdptask_thread_pool[i] = NULL;
		}
	}
	systhread_mutex_free(s_cdptask_mutex);
}

void cdptask_threadproc(void)
{
	t_cdptask *r;

	while (true) {
		if (cdptask_getrequest(&r)<0) {
			systhread_sleep(cdpTASK_SLEEPTIME); // could change this number dynamically, or use condvars
		} else {
			if (r) {
				r->state = cdpTASK_REQ_PROCESSING;
				cdptask_completerequest(r);
			}
		}
	}
}

long cdptask_makerequest(t_cdptask *task)
{
	long rv=-1;

	if (s_cdptask_exit)
		return -1;

	cdpTASK_MUTEX_LOCK;
	linklist_append(s_cdptask_requestlist,task);
	s_cdptask_numrequests++;
	cdpTASK_MUTEX_UNLOCK;

	rv = 0;

	return rv;
}

long cdptask_getrequest(t_cdptask **task)
{
	long  rv=-1;

	cdpTASK_MUTEX_LOCK;
	if (s_cdptask_numrequests>0) {
		*task = (t_cdptask *)linklist_getindex(s_cdptask_requestlist,0);
		linklist_chuckindex(s_cdptask_requestlist,0);
		linklist_append(s_cdptask_executelist,*task);
		s_cdptask_numrequests--;
		rv = 0;
	} else {
		*task = NULL;
	}
	cdpTASK_MUTEX_UNLOCK;
	return rv;
}

void cdptask_completerequest(t_cdptask *task)
{
	if (task&&task->cbtask) {
		(*((method)task->cbtask))(task->owner,task->args,task);
	}
	task->state = cdpTASK_REQ_COMPLETE; // redundant to set state
	if (task&&task->cbcomplete) {
		(*((method)task->cbcomplete))(task->owner,task->args,task);
	}

	cdpTASK_MUTEX_LOCK;
	linklist_chuckobject(s_cdptask_executelist,task);
	cdpTASK_MUTEX_UNLOCK;

	sysmem_freeptr(task);
}

typedef struct _cdptask_requestmatch
{
	t_object	*owner;
	t_linklist	*list;
} t_cdptask_requestmatch;

void cdptask_requestmatch_fn(t_cdptask *task, t_cdptask_requestmatch *match);
void cdptask_requestmatch_fn(t_cdptask *task, t_cdptask_requestmatch *match)
{
	t_cdptask_method_caller *caller;

	if (task&&match) {
		if (task->owner==match->owner)
			linklist_append(match->list,task);
		if ((void *)task->cbtask==(void *)cdptask_method_caller_task) {
			caller = (t_cdptask_method_caller *)task->owner;
			if (caller->obtask==match->owner || caller->obcomp==match->owner)
				linklist_append(match->list,task);
		}
	}
}

t_linklist *cdptask_object_requestlist(t_object *owner);
t_linklist *cdptask_object_requestlist(t_object *owner)
{
	t_linklist *list=NULL;
	t_cdptask_requestmatch match;

	list = linklist_new();
	linklist_flags(list,OBJ_FLAG_REF);
	match.owner = owner;
	match.list = list;

	cdpTASK_MUTEX_LOCK;
	linklist_funall(s_cdptask_requestlist,(method)cdptask_requestmatch_fn,(void *)&match);
	linklist_funall(s_cdptask_executelist,(method)cdptask_requestmatch_fn,(void *)&match);
	cdpTASK_MUTEX_UNLOCK;

	return list;
}

void cdptask_purge_object(t_object *owner)
{
	t_linklist *list;
	t_cdptask *task;

	// build linklist of requests whose owner pointer matches
	list = cdptask_object_requestlist(owner);
	// call cdptask_cancel on each one
	while ((task = (t_cdptask *)linklist_getindex(list,0))) {
		cdptask_cancel(task);
		linklist_chuckindex(list,0);
	}
	linklist_chuck(list);
}

void cdptask_join_object(t_object *owner)
{
	t_linklist *list;
	t_cdptask *task;

	// build linklist of requests whose owner pointer matches
	list = cdptask_object_requestlist(owner);
	// call cdptask_join on each one
	while ((task = (t_cdptask *)linklist_getindex(list,0))) {
		cdptask_join(task);
		linklist_chuckindex(list,0);
	}
	linklist_chuck(list);
}

long cdptask_cancel(t_cdptask *task)
{
	long i,wait=FALSE;
	long rv = -1;

	cdpTASK_MUTEX_LOCK;
	i = linklist_objptr2index(s_cdptask_requestlist,task);
	if (i>=0) {
		linklist_chuckindex(s_cdptask_requestlist,i);
		if ((void *)task->cbtask==(void *)cdptask_method_caller_task) {
			cdptask_method_caller_free((t_cdptask_method_caller *)task->owner);
		}
		sysmem_freeptr(task);
		rv = 0; // found and cancelled
	} else {
		i = linklist_objptr2index(s_cdptask_executelist,task);
		wait = (i>=0);
		rv = 1; // found and joined
	}
	cdpTASK_MUTEX_UNLOCK;

	// if the task is executing, stall
	while (wait) {
		systhread_sleep(cdpTASK_SLEEPTIME);
		i = linklist_objptr2index(s_cdptask_executelist,task);
		wait = (i>=0);
	}
	return rv;
}


long cdptask_join(t_cdptask *task)
{
	long i,j,wait=TRUE;
	long rv = -1;

	while (wait) {
		cdpTASK_MUTEX_LOCK;
		i = linklist_objptr2index(s_cdptask_requestlist,task);
		j = linklist_objptr2index(s_cdptask_executelist,task);
		if (i>=0 || j>=0) {
			wait = TRUE;
			rv = 0; // found and joined
		} else {
			wait = FALSE;
		}
		cdpTASK_MUTEX_UNLOCK;
		if (wait)
			systhread_sleep(cdpTASK_SLEEPTIME);
	}
	return rv;
}

long cdptask_execute(t_object *owner, void *args, method cbtask, method cbcomplete, t_cdptask **task, long flags)
{
	long err=-1;
	t_cdptask *bgt;

	if (!s_cdptask_init)
		cdptask_init();

	if ((bgt=(t_cdptask *)sysmem_newptr(sizeof(t_cdptask)))) {
		// store flags and permissions for later use
		bgt->flags = flags;
		bgt->state = cdpTASK_REQ_PENDING;
		bgt->id = s_cdptask_id;
		bgt->owner = owner;
		bgt->cbtask = cbtask;
		bgt->cbcomplete = cbcomplete;
		bgt->args = args;

		if (task)
			*task = bgt;

		return cdptask_makerequest(bgt);
	}

	return err;
}


void cdptask_method_caller_task(t_cdptask_method_caller *x)
{
	if (x&&x->obtask)
		object_method_typed(x->obtask,x->mtask,x->actask,x->avtask,NULL);
}

void cdptask_method_caller_complete(t_cdptask_method_caller *x)
{
	if (x&&x->obcomp)
		object_method_typed(x->obcomp,x->mcomp,x->accomp,x->avcomp,NULL);
	cdptask_method_caller_free(x);
}

void cdptask_method_caller_free(t_cdptask_method_caller *x)
{
	if (x) {
		if (x->avtask)
			sysmem_freeptr(x->avtask);
		if (x->avcomp)
			sysmem_freeptr(x->avcomp);
		sysmem_freeptr(x);
	}
}


long cdptask_execute_method(t_object *obtask, t_symbol *mtask, long actask, t_atom *avtask,
								   t_object *obcomp, t_symbol *mcomp, long accomp, t_atom *avcomp,  t_cdptask **task, long flags)
{
	long i;
	t_cdptask_method_caller *x;

	x = (t_cdptask_method_caller *) sysmem_newptr(sizeof(t_cdptask_method_caller));

	if (x) {
		x->obtask = obtask;
		x->mtask = mtask;
		x->actask = actask;
		if (x->actask>0)
			x->avtask = (t_atom *) sysmem_newptr(actask*sizeof(t_atom));
		else
			x->avtask = NULL;
		for (i=0; i<x->actask; i++)
			x->avtask[i] = avtask[i];

		x->obcomp = obcomp;
		x->mcomp = mcomp;
		x->accomp = accomp;
		if (x->accomp>0)
			x->avcomp = (t_atom *) sysmem_newptr(accomp*sizeof(t_atom));
		else
			x->avcomp = NULL;
		for (i=0; i<x->accomp; i++)
			x->avcomp[i] = avcomp[i];

		return cdptask_execute((t_object *)x,(void *)NULL,(method)cdptask_method_caller_task,(method)cdptask_method_caller_complete,task,flags);
	}

	return -1;
}



