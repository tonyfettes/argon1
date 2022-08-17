# argon1

Linux driver for Argon ONE case.

This driver exposes i2c device as `/sys/class/thermal/cooling_device[0-*]`,
which could be utilized by various thermal governor.

The power button is not handled in this driver since it can be mapped to input
key by module `gpio-keys`.

## Roadmap

- [x] Basic fan support.
- [ ] Emulate lower speed (1-9 out of 100) by issuing command periodically.
- [ ] Add device tree nodes register GPIO pin as `KEY_POWER`.
- [ ] Add GPIO driver that recognize `KEY_POWER` and `KEY_RESTART`.
