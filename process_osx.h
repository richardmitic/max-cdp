//
//  process.h
//  cdp
//
//  Created by Richard Mitic on 27/02/2017.
//
//

#ifndef process_osx_h
#define process_osx_h

#define PROCESS_OUTPUT_MAX_SIZE (1024)

int run_process(char *cmd, char *stdout, int stdout_size);
int get_tmp_file_name(char *buf, int buflen, char *prefix);

#endif /* process_osx_h */
