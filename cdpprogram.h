//
//  cdpprogram.h
//  cdp
//
//  Functions to run each CDP executable
//
//  Created by Richard Mitic on 28/02/2017.
//
//

#ifndef cdpprogram_h
#define cdpprogram_h

#include "ext.h"
#include "ext_obex.h"
#include "cdp.h"

t_max_err cdp_find_excutable(t_cdp *x, char *name, short *path, char *full_path);
t_max_err cdp_make_cmd(t_cdp *x, char *executable, long ac, t_atom *av, t_string **cmd);
void cdp_do_program(t_cdp *x, char *program, long ac, t_atom *av);
char *get_filename_ext(char *filename);

#endif /* cdpprogram_h */
