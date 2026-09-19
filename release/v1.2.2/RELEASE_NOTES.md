## reVita 1.2.2

### Menu in Adrenaline with "Original" graphics filtering
With Graphics Filtering set to Original and no GePatch, Adrenaline shows PSP frames through a separate display path that reVita never saw, so the menu did not appear. reVita now hooks that path and draws the menu (and popups) straight into the PSP frame. Filtered modes and GePatch keep working as before. This path is new and untested on hardware; if the menu looks wrong there, switch Graphics Filtering away from Original as before and report it.

### The "Buy more RAM !" popup is gone
The menu used to allocate a 512 KB kernel memory block every time it opened. On systems with other heavy kernel plugins (ucdc, PSVShell) that allocation failed and the menu refused to open. The menu now draws into a small static buffer inside the plugin (64 KB, palette indexed), so opening it allocates nothing and cannot fail.

### Less kernel RAM
Measured on the kernel module: 235852 bytes resident and no runtime memory blocks, versus 277660 bytes with the menu closed and 801948 bytes with it open in 1.1.2. The input history cache is static (the never-used half for the touch region hooks is gone), the unused 16 KB log buffer and duplicated font tables were removed, and the module is built with -Os.

### Crash fixes
- Saving a profile with about 20 or more rules overflowed a 4 KB kernel buffer; the INI writer is now bounded and the profile buffer is 12 KB. A save that cannot be written shows "Profile NOT saved" instead of failing silently.
- INI parser: field widths now match their buffers, no recursion per section line, empty theme values are safe, files are always NUL-terminated.
- An unknown action name in a profile file dereferenced NULL (the action name table was one entry short).
- Seven simultaneous emulated touch points overran a 6-entry array.
- Save backup/restore: fixed error cleanup (leaked file descriptors) and use one 32 KB buffer per copied tree instead of 128 KB per file.
- The "Savegame backup" and "Savegame restore" remap actions were swapped.

### Build
Ported to the current VitaSDK headers; `-Wl,--defsym=__sce_headroom` so the headless module links; verbose logging is an opt-in CMake option. See `docs/MEMORY_REVIEW.md` for the full review and measurements.

### Files
- `reVita.skprx`: kernel plugin (ur0:/tai, KERNEL section, before ds34vita/ds4touch)
- `reVitaHeadless.skprx`: kernel plugin without the menu
- `reVitaMotion.suprx`: optional user module for gyro support (ur0:/tai, MAIN section)

Not yet tested on hardware: built and reviewed from source only. Please report menu rendering or remapping problems.
