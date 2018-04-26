
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/unistd.h>
#include <k_api.h>

#include "common.h"

#undef errno
extern int errno;

#ifndef EBADF
#include <errno.h>
#endif

