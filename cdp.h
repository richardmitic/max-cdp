//
//  cdp.h
//  cdp
//
//  Created by Richard Mitic on 28/02/2017.
//
//

#ifndef cdp_h
#define cdp_h

typedef struct _cdp {
  t_object  x_ob;      // standard max object
  void     *x_outlet;  // our outlet
  t_symbol *cdp_path;  // Path to CDP executables
  long allocated_memory;
} t_cdp;

#endif /* cdp_h */
