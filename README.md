# vita-ps4linkcontrols

taiHEN plugin that allows to force preferred Remote Play button configuration

### Configuration

There are currently 2 options that can be set in configuration file `ux0:ps4linkcontrols.txt`.
First line should be keymap number and second line controller type.

For example, to set keymap = 2 and controller_type = 3, configuration file should contain:

```
2
3
```

#### Keymap

Keymap is a layout how are the additional buttons not present on Vita (L2/R2 and L3/R3) are mapped
on either back or front Vita touch pad.

By default it sets **keymap 0** - everything on back touchpad.
Available keymaps are in `vs0:app/NPXS10013/keymap/`(folder name is the keymap number).

#### Controller Type

Controller Type is how PS4Link app identifies itself when connecting to PS4. By changing this value
it is possible to disable Vita remote play controls customization for some PS4 games. Remote Play client
is available for multiple platforms (which each probably have its own type number), so you may try to experiment
with different values.  
Setting this value to `3` (which is Windows Remote Play client) is confirmed to fix Metal Gear Solid V: The Phantom Pain
where it would otherwise remap Vita L/R triggers as L2/R2.

By default it sets **controller type 1** - default Vita value.

### Installation

Add this plugin under `*NPXS10013` section in `ux0:tai/config.txt`:

```
*NPXS10013
ux0:tai/ps4linkcontrols.suprx
```
