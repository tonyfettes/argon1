# argon1

Linux driver for Argon ONE case.

This driver expose i2c devices as `/sys/class/thermal/cooling_device[0-*]`,
which could be utilized by various thermal governor.
