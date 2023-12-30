#ifndef __TYPES_H__
#define __TYPES_H__

#include <inttypes.h>

namespace Profiler {

typedef uint64_t addr_t;
typedef uint64_t reg_t;

#define pprintf(...)                       \
  fprintf(stdout, "[prof] " __VA_ARGS__ ); \
  fflush(stdout);



#define k_do_execveat_common "do_execveat_common.isra.0"
#define k_do_execveat_common_filename_arg 1

#define k_set_mm_asid "set_mm_asid"


#define XPR_CNT 32

} // namespace Profiler

#endif //__TYPES_H__
