# Sintekek-rtsx

This is a try to make this kext working on my laptop (Dell XPS 9350). All credits are due to the original authors, @sinetek and @syscl.

## Overall notes

It took me a while to understand the code. Some problems I found:

1. We needed a way to wait for the interrupt while in `workloop_`. The solution adopted is to have two workloops, the normal `workloop_` and a separate workloop for tasks. I'm not sure whether this approach is right or not (an `IOCommandGate` with one single workloop seems the way to go), but for now it seems to work.
1. It seems that the OpenBSD driver is still in a very rough state. My hope is that this driver gets improved over time and that these changes can be incorporated here.

## Changes made

An OpenBSD-compatibility layer has been added to make the original OpenBSD driver work with as few changes as possible. This implied rewriting all OpenBSD functions which are not available in Darwin so that the same behavior is obtained using only functions available in the macOS kernel. The benefit this brings is that the future improvements in the OpenBSD driver can be incorporated more easily.

### Compile-Time Options

The code allows some customization by defining/undefining certain preprocessor macros (set on ):

| Option                   | Requires          | Notes                                                                                                                       |
|--------------------------|-------------------|-----------------------------------------------------------------------------------------------------------------------------|
| `RTSX_FIX_TASK_BUG`      |                   | Fix task bug (reusing task pointer instead of allocating it).                                                               |
| `RTSX_USE_WRITEBYTES`    |                   | Use `IOMemoryDescriptor::writeBytes()` method to write buffer instead of the original `buffer->map()->getVirtualAddress()`. |
| `RTSX_USE_IOFIES`        |                   | Use `IOFilterInterruptEventSource` instead of `IOInterruptEventSource` (should give better performance).                    |
| `RTSX_USE_IOLOCK`        |                   | This should use more locks to protect critical sections.                                                                    |
| `RTSX_USE_IOCOMMANDGATE` | `RTSX_USE_IOLOCK` | A try to make `IOCommandGate` working, but never really worked.                                                             |
| `RTSX_USE_IOMALLOC`      |                   | Use `IOMalloc`/`IOFree` for memory management instead of `new`/`delete`.                                                    |

### Boot Arguments

| Option              | Notes                                                                                                                       |
|---------------------|-----------------------------------------------------------------------------------------------------------------------------|
| `-rtsx_mimic_linux` | Do some extra initialization which may be useful if your chip is exactly RTS525A version B (exactly the same as mine).      |
| `-rtsx_ro`          | Read-only mode (disable writing).                                                                                           |

## To Do

From higher to lower priority:

 - Make use of ADMA: This is enabled in OpenBSD, but has been disabled for now because it needs some changes in the compatibility layer (support for dma-maps with more than one segment).
 - Fine-tune timeouts? (these may go away once power management is finished).
 - Use command gate instead of two workloops? Is it even possible?
 - Prevent namespace pollution (OpenBSD functions pollute the namespace and may cause collisions).

Pull requests are very welcome, specially to add support for chips other than RTS525A (the only chip I can test).
