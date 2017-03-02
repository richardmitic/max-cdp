#!/usr/bin/env python3

import argparse
import os

SOURCE = """
// {filename}
// This file was generated automatically by {script}.

{includes}

{code}
"""

HEADER = """
// {filename}
// This file was generated automatically by {script}.

#ifndef {tag}
#define {tag}

{inner}

#endif // {tag}
"""

CDP_DO_METH_DECL = """void cdp_do{prog}(t_cdp *x, t_symbol *s, long ac, t_atom *av);"""

CDP_DO_METH_IMPL = """void cdp_do{prog}(t_cdp *x, t_symbol *s, long ac, t_atom *av) {{cdp_do_program(x, "{prog}", ac, av);}}"""

CLASS_METH = """class_addmethod(c, (method)cdp_{prog}, "{prog}", A_GIMME, 0);"""
CLASS_DO_METH = """class_addmethod(c, (method)cdp_do{prog}, "do{prog}", A_GIMME, 0);"""

CDP_METH_DECL = """void cdp_{prog}(t_cdp *x, t_symbol *s, long ac, t_atom *av);"""

CDP_METH_IMPL = """\
void cdp_{prog}(t_cdp *x, t_symbol *s, long ac, t_atom *av)
{{
  cdptask_execute_method((t_object *)x,gensym("do{prog}"),ac,av,
                         (t_object *)x,gensym("taskcomplete"),ac,av,
                         NULL,0);
}}\
"""

def get_progs(root_dir):
    root, dirs, files = next(os.walk(root_dir, topdown='true'))
    progs = [f for f in files if os.access(os.path.join(root, f), os.X_OK) and f[-3:]!='.sh']
    return root, progs


def make_header(name, inner_txt):
    tag = name.upper().replace('.', '_')
    return HEADER.format(tag=tag, filename=name, script=os.path.basename(__file__), inner=inner_txt)

def write_standard_header(path, progs, template):
    txt = "\n".join([template.format(prog=p) for p in progs])
    name = os.path.basename(path)
    with open(path, 'w') as f:
        f.write(make_header(name, txt))

def write_macro_header_arg(path, progs, template, macro):
    methods = "\n".join([template.format(prog=p) + "\\" for p in progs])
    txt = "#define {macro}(c) \\\n {methods}".format(macro=macro, methods=methods)
    name = os.path.basename(path)
    with open(path, 'w') as f:
        f.write(make_header(name, txt))

def write_source_file(path, progs, template, includes=[]):
    methods = "\n\n".join([template.format(prog=p) for p in progs])
    headers = "\n".join(["#include \"{}\"".format(i) for i in includes])
    name = os.path.basename(path)
    txt = SOURCE.format(filename=name, script=os.path.basename(__file__), includes=headers, code=methods)
    with open(path, 'w') as f:
        f.write(txt)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Parse arguments for ')
    parser.add_argument('-p', '--cdp-path', type=str, help='Path to CDP directory')

    args = parser.parse_args()
    root, progs = get_progs(args.cdp_path)

    header_dir = "generated_headers"
    sources_dir = "generated_sources"
    os.makedirs(header_dir, exist_ok=True)
    os.makedirs(sources_dir, exist_ok=True)

    write_standard_header(os.path.join(header_dir, "cdp_methods.h"), progs, CDP_METH_DECL)
    write_standard_header(os.path.join(header_dir, "cdp_domethods.h"), progs, CDP_DO_METH_DECL)
    write_macro_header_arg(os.path.join(header_dir, "class_methods.h"), progs, CLASS_METH, "CLASS_ADD_CDP_METHODS")
    write_macro_header_arg(os.path.join(header_dir, "class_domethods.h"), progs, CLASS_DO_METH, "CLASS_ADD_CDP_DO_METHODS")
    write_source_file(os.path.join(sources_dir, "cdp_methods_impl.c"), progs, CDP_METH_IMPL, includes=["ext.h", "ext_obex.h", "cdp.h", "cdptask.h"])
    write_source_file(os.path.join(sources_dir, "cdp_domethods_impl.c"), progs, CDP_DO_METH_IMPL, includes=["ext.h", "ext_obex.h", "cdp.h", "cdpprogram.h"])
