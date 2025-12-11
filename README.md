# Total Registry

Replacement for the Windows built-in *Regedit.exe* tool. Improvements over that tool include:

* Show real Registry (not just the standard one)
* Sort list view by any column
* Key icons for hives, inaccessible keys, and links
* Key details: last write time and number of keys/values
* Displays MUI and REG_EXPAND_SZ expanded values
* Full search (Find All / Ctrl+Shift+F)
* Enhanced hex editor for binary values
* Undo/redo
* Copy/paste of keys/values
* Optionally replace RegEdit
* Connect to remote Registry
* View open key handles

## Build Instructions

### Prerequisites
- Visual Studio 2022 (or 2019 with toolset v142)
- [vcpkg](https://vcpkg.io/) package manager

### Steps

1. **Clone with submodules:**
   ```bash
   git clone --recursive https://github.com/zodiacon/TotalRegistry.git
   cd TotalRegistry
   ```

2. **Install dependencies via vcpkg:**
   ```bash
   vcpkg install
   ```
   Or enable vcpkg manifest mode in Visual Studio project properties.

3. **Build:**
   Open `TotalRegistry.sln` in Visual Studio 2022 and build (Debug/Release x64).

### Dependencies
- [detours](https://github.com/microsoft/Detours) - Microsoft Detours library
- [wil](https://github.com/microsoft/wil) - Windows Implementation Libraries
- [WTL 10](https://github.com/allenk/WTL-Wizard-VS2022) - Windows Template Library headers

### Notes
- Can be built with Visual Studio 2019 (change toolset to v142 and C++ compiler version to C++20).

![](https://github.com/zodiacon/RegExp/blob/master/regexp1.png)

![](https://github.com/zodiacon/RegExp/blob/master/regexp2.png)

![](https://github.com/zodiacon/RegExp/blob/master/regexp3.png)
