#ifndef __TYPES_H__
#define __TYPES_H__

#include <inttypes.h>

namespace Profiler {

typedef uint64_t addr_t;
typedef uint64_t reg_t;

#define pprintf(...) printf( "[prof] " __VA_ARGS__ )



#define k_alloc_bprm "alloc_bprm"
#define k_alloc_bprm_filename_arg 1

} // namespace Profiler

#endif //__TYPES_H__
