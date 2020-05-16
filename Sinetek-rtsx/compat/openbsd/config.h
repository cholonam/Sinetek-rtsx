#ifndef SINETEK_RTSX_COMPAT_OPENBSD_CONFIG_H
#define SINETEK_RTSX_COMPAT_OPENBSD_CONFIG_H

#include <sys/cdefs.h> // __BEGIN_DECLS, __END_DECLS

#include "types.h" // type definitions needed

__BEGIN_DECLS
#define _KERNEL
#include "device.h" // function declarations in device.h
__END_DECLS

#endif // SINETEK_RTSX_COMPAT_OPENBSD_CONFIG_H
