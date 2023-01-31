# qt5-kobo-platform-plugin
### Original by Rain92, thanks for the magic.
A Qt5 platform backend plugin for Kobo E-Ink devices.

Supports:
- E-Ink screen refresh control,
- Touchscreen support

Deleted by InkBox project:
- Button input management - Managed by IPD
- frontlight control - Managed by higher level software and IPD
- WiFi management - Managed by InkBox
- Other functions

Added:
- Full support for keyboard and mouse (working *good*)
- this cool readme
- Qt Creator support
- Repaired rotation

At runtime an app using this plugin can be configurend with the environment variable QT_QPA_PLATFORM. \
The following parameters are supported:
- debug - enables additional debug output 
- logicaldpitarget= - enables high dpi scaling 
- keyboard - enables keyboard support
- mouse - enables keyboard support
- motiondebug - enabled additional debug - focused on movement / refreshing. Mainly for mouse

For example:
```
export QT_QPA_PLATFORM=kobo:debug:logicaldpitarget=108:keyboard:mouse:motiondebug ./myapp
```

## Cross-compile for Kobo
See ~~https://github.com/Rain92/UltimateMangaReader~~ https://github.com/Szybet/niAudio/blob/main/apps-on-kobo/qt-setup.md
