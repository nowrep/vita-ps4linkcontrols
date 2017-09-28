# vita-ps4linkcontrols

taiHEN plugin that allows to force preferred Remote Play button configuration

### Configuration

By default it sets **keymap 0** - everything on back touchpad. Changing the keymap can be done
by creating `ux0:ps4linkcontrols.txt` file and writing the keymap number (0-7) there.

### Installation

Add this plugin under `*NPXS10013` section in `ux0:tai/config.txt`:

```
*NPXS10013
ux0:tai/ps4linkcontrols.suprx
```
