#ifndef SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_BUS_SPACE_H
#define SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_BUS_SPACE_H

#include "openbsd_compat_types.h"

u_int32_t bus_space_read_4(bus_space_tag_t space, bus_space_handle_t handle, bus_size_t offset);

void bus_space_write_4(bus_space_tag_t space, bus_space_handle_t handle, bus_size_t offset, u_int32_t value);

#endif // SINETEK_RTSX_OPENBSD_OPENBSD_COMPAT_BUS_SPACE_H
