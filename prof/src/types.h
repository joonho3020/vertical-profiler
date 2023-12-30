#ifndef __TYPES_H__
#define __TYPES_H__

#include <inttypes.h>

namespace Profiler {


/////////////////////////////////////////////////////
// MACROS
/////////////////////////////////////////////////////
#define   likely(x) __builtin_expect(x, 1)
#define unlikely(x) __builtin_expect(x, 1)


#define pprintf(...)                       \
  fprintf(stdout, "[prof] " __VA_ARGS__ ); \
  fflush(stdout);



/////////////////////////////////////////////////////
// typedefs
/////////////////////////////////////////////////////
typedef uint64_t addr_t;
typedef uint64_t reg_t;




/////////////////////////////////////////////////////
// consts
/////////////////////////////////////////////////////
#define k_do_execveat_common "do_execveat_common.isra.0"
#define k_do_execveat_common_filename_arg 1

#define k_set_mm_asid "set_mm_asid"

#define KERNEL "k"

#define XPR_CNT 32

} // namespace Profiler

#endif //__TYPES_H__
