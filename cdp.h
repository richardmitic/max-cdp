//
//  cdp.h
//  cdp
//
//  Created by Richard Mitic on 28/02/2017.
//
//

#ifndef cdp_h
#define cdp_h

#define CDP_INPUT_PREFIX  "cdpin"
#define CDP_INPUT_PREFIX_SIZE (5)
#define CDP_OUTPUT_PREFIX "cdpout"
#define CDP_OUTPUT_PREFIX_SIZE (6)

typedef struct _cdp {
  t_object  x_ob;      // standard max object
  void     *x_outlet;  // our outlet
  t_symbol *cdp_path;  // Path to CDP executables
  bool      overwrite; // Overwrite output files without warning
} t_cdp;

#endif /* cdp_h */
