# Sintekek-rtsx

This is a try to make this kext working on my laptop (Dell XPS 9350). All credits are due to the original authors, @sinetek and @syscl.

## Overall notes

It took me a while to understand the code. Some problems I found:

1. We needed a way to wait for the interrupt while in `workloop_`. The solution adopted is to have two workloops, the normal `workloop_` and a separate workloop for tasks. I'm not sure whether this approach is right or not (an `IOCommandGate` with one single workloop seems the way to go), but for now it seems to work.
1. It seems that the OpenBSD driver is still in a very rough state. My hope is that this driver gets improved over time and that these changes can be incorporated here.
1. Currently, stability for RTS525A has been improved up to a usable level. ***This does not apply to other chips, which may have a very unstable behavior or not work at all.***

### Chips known to work

| Chip No. | Notes                                                                                                                                           |
|----------|-------------------------------------------------------------------------------------------------------------------------------------------------|
| RTS5209  | Reported to work. (See issue #28)                                                                                                               |
| RTS5227  | Seems to work fine with sleep disabled. Adding boot parameter `rtsx_sleep_wake_delay_ms=1000` may help with sleep/wake. (See PR #18)            |
| RTS522A  | <s>Working fine with sleep disabled. Not working after sleep and wakes up sleep constantly.</s><br/>Adding boot parameter `rtsx_sleep_wake_delay_ms=1000` makes it work with sleep enabled. (See issue #27) |
| RTS525A  | Working fine with sleep disabled. Enabling sleep may make the kext unstable. Some cards may not be recognized.                                  |
| RTS5287  | Working fine with sleep disabled. Not waking from sleep. (See issue #19)                                                                        |

 _If you have a chip other than RTS525A and this kext is working for you, please let me know and I will update this table._

## Changes made

* An OpenBSD-compatibility layer has been added to make the original OpenBSD driver work with as few changes as possible. This implied rewriting all OpenBSD functions which are not available in Darwin so that the same behavior is obtained using only functions available in the macOS kernel. The benefit this brings is that the future improvements in the OpenBSD driver can be incorporated more easily.
* Use `IOFilterInterruptEventSource` instead of `IOInterruptEventSource` (should give better performance).
* Fixed a bug where a single task member was being reused. Since there may be more than one task pending, a new task struct must be allocated/freed for each new task.

### Compile-Time Options

The code allows some customization by defining/undefining certain preprocessor macros (set on ):

| Option                   | Requires          | Notes                                                                                                                       |
|--------------------------|-------------------|-----------------------------------------------------------------------------------------------------------------------------|
| `RTSX_USE_IOLOCK`        |                   | This should use more locks to protect critical sections.                                                                    |
| `RTSX_USE_IOCOMMANDGATE` | `RTSX_USE_IOLOCK` | A try to make `IOCommandGate` working, but never really worked.                                                             |
| `RTSX_USE_IOMALLOC`      |                   | Use `IOMalloc`/`IOFree` for memory management instead of `new`/`delete`.                                                    |
| `RTSX_USE_PRE_ERASE_BLK` |                   | Issue an ACMD23 before a multiblock write. Does not seem to make any difference in speed. Disabled by default.              |

### Boot Arguments

| Option                       | Notes                                                                                                                       |
|------------------------------|-----------------------------------------------------------------------------------------------------------------------------|
| `-rtsx_mimic_linux`          | Do some extra initialization which may be useful if your chip is exactly RTS525A version B (exactly the same as mine).      |
| `-rtsx_no_adma`              | Disable ADMA.                                                                                                               |
| `-rtsx_ro`                   | Read-only mode (disable writing).                                                                                           |
| `rtsx_timeout_shift=n`       | Multiply timeouts times 2<sup>*n*</sup>. May help with some slow cards (i.e.: `rtsx_timeout_shift=2`).                      |
| `rtsx_sleep_wake_delay_ms=n` | Introduce a delay on sleep/wake that may help with some chips like RTS5227.                      |

## Known Issues / Troubleshooting

1. Slow performance
   The driver only supports up to High-Speed mode, meaning that UHS-I and higher cards will only work as HS. This limitation comes from the OpenBSD driver on which this kext is based and I do not have any plan to fix it.

1. Kext not unloading
   You should be able to unload the kext using the command `kextunload -c Sinetek_rtsx`. Possible error causes are:
   1. The card is inserted.
   1. Some user-level program (HWMonitor is one of them) may hold references to a class in this kext that would prevent unloading. Try terminating these programs.

1. Sleep/Wake Issues
   The card is unmounted on sleep and remounted on wake. This is the expected behavior and it should work at least for chip RTS525A. For other chips, the card may become unreadable upon wake. If adding the `rtsx_sleep_wake_delay_ms=1000` boot parameter solves your sleep/wake issues, and your chip *is not* RTS5227, please let me know so that I can update the above table.

## To Do

From higher to lower priority:

 - Troubleshoot why after a soft reboot, for chip 525A version B, chip version is detected as 'A'.
 - Use command gate instead of two workloops? Is it even possible?
 - Prevent namespace pollution (OpenBSD functions pollute the namespace and may cause collisions).

Pull requests are very welcome, specially to add support for chips other than RTS525A (the only chip I can test).
