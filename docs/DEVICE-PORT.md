# Future device port

Create a Linux `Platform` implementation and replace `createPlatform("desktop")` at
the composition root. The application manager, services, configuration, sandbox, and
SDL shell remain unchanged. A board backend should source battery from sysfs, Wi-Fi
from the selected network manager, Bluetooth from BlueZ, camera frames from V4L2 or
SDL Camera, and USB state from the board's gadget controller.

The included systemd unit expects a cross-compiled binary at `/opt/pine-os/bin/pine`
and persistent data at `/var/lib/pine-os`. Bootloader, kernel, DTB, compositor, and
USB gadget policy remain board-specific and must be selected only after hardware is
known.
