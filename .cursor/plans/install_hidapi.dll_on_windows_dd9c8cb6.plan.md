---
name: Install hidapi.dll on Windows
overview: The "hidapi.dll was not found" error occurs because the DLL is only copied next to the exe in the build tree; the install step never installs it to `build/install/`. Add an install rule for the hidapi DLL on Windows when the Emotiv app is built.
todos: []
isProject: false
---

# Fix missing hidapi.dll when running from install directory

## Root cause

- - 

