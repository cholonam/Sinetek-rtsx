#include "openbsd_compat_bus_space.h"

#include <IOKit/IOMemoryDescriptor.h>

#define UTL_THIS_CLASS ""
#include "util.h"

/// We will use bus_space_handle_t as a IOMemoryDescriptor
typedef IOMemoryDescriptor *_bus_space_handle_t;

u_int32_t bus_space_read_4(bus_space_tag_t space, bus_space_handle_t handle, bus_size_t offset)
{
	u_int32_t ret;
	_bus_space_handle_t memDesc = (_bus_space_handle_t) handle;
	UTL_CHK_PTR(memDesc, 0);
	memDesc->prepare();
	memDesc->readBytes((IOByteCount) offset, &ret, 4);
	memDesc->complete();
	if (offset != 0x10)
		UTL_DEBUG_CMD("BUS SPACE READ:  off 0x%02x => 0x%08x (%s)", (int) offset, (int) ret,
			      busSpaceReg2str(offset));
	return ret;
}

void bus_space_write_4(bus_space_tag_t space, bus_space_handle_t handle, bus_size_t offset, u_int32_t value)
{
	_bus_space_handle_t memDesc = (_bus_space_handle_t) handle;
	UTL_CHK_PTR(memDesc,);
	if (offset != 0x10)
		UTL_DEBUG_CMD("BUS SPACE WRITE: off 0x%02x <= 0x%08x (%s)", (int) offset, (int) value,
			      busSpaceReg2str(offset));
	memDesc->prepare();
	memDesc->writeBytes((IOByteCount) offset, &value, 4);
	memDesc->complete();
}
