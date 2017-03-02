// cdptask.h
#ifndef __cdpTASK_H__
#define __cdpTASK_H__

#if C74_PRAGMA_STRUCT_PACKPUSH
#pragma pack(push, 2)
#elif C74_PRAGMA_STRUCT_PACK
#pragma pack(2)
#endif

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct _cdptask
{
	long				flags; // rfu
	long				state;
	long				id;
	t_object			*owner;
	void				*args;
	method				cbtask;
	method				cbcomplete;
} t_cdptask;

void cdptask_init(void);
long cdptask_execute(t_object *owner, void *args, method cbtask, method cbcomplete, t_cdptask **task, long flags);
long cdptask_execute_method(t_object *obtask, t_symbol *mtask, long actask, t_atom *avtask,
								   t_object *obcomp, t_symbol *mcomp, long accomp, t_atom *avcomp,  t_cdptask **task, long flags);
void cdptask_purge_object(t_object *owner);
void cdptask_join_object(t_object *owner);
long cdptask_cancel(t_cdptask *task);
long cdptask_join(t_cdptask *task);


#ifdef __cplusplus
}
#endif // __cplusplus

#if C74_PRAGMA_STRUCT_PACKPUSH
#pragma pack(pop)
#elif C74_PRAGMA_STRUCT_PACK
#pragma pack()
#endif


#endif // __cdpTASK_H__
