//🫖ketl
#ifndef ketl_compiler_bnf_scheme_h
#define ketl_compiler_bnf_scheme_h

#include "ketl/utils.h"

KETL_FORWARD(ketl_bnf_node);
KETL_FORWARD(ketl_object_pool);

ketl_bnf_node* ketl_bnf_build_scheme(ketl_object_pool* bnfNodePool);

#endif // ketl_compiler_bnf_scheme_h
