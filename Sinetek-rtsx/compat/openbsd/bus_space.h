#ifndef SINETEK_RTSX_COMPAT_OPENBSD_BUS_SPACE_H
#define SINETEK_RTSX_COMPAT_OPENBSD_BUS_SPACE_H

#include <sys/cdefs.h> // __BEGIN_DECLS, __END_DECLS

#include "types.h"

__BEGIN_DECLS

u_int32_t bus_space_read_4(bus_space_tag_t space, bus_space_handle_t handle, bus_size_t offset);

void bus_space_write_4(bus_space_tag_t space, bus_space_handle_t handle, bus_size_t offset, u_int32_t value);

__END_DECLS

#endif // SINETEK_RTSX_COMPAT_OPENBSD_BUS_SPACE_H
