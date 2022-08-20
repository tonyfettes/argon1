# argon1

Linux driver for Argon ONE case.

This driver exposes i2c device as `/sys/class/thermal/cooling_device[0-*]`,
which could be utilized by various thermal governor. Also, it exposes power
button through input subsystem.

## Roadmap

- [x] Basic fan support.
- [ ] Emulate lower speed (1-9 out of 100) by issuing command periodically.
- [x] Add device tree nodes register GPIO pin as `KEY_POWER`.
- [x] Add GPIO driver that recognize `KEY_POWER` and `KEY_RESTART`.
- [ ] Merge two drivers into one module.
