# Callback Manager Mod
This mod provides an interface where other mods can register before and after functions with code events and script functions.
This allows for better support for running multiple mods at once because the callback manager will ensure that the original code event/script function will only run at most once.
## Usage Steps
- Download `CallbackManagerMod.dll` and `CallbackManagerInterface.h` from the latest version of the mod https://github.com/PippleCultist/CallbackManagerMod/releases
  - Put `CallbackManagerMod.dll` in the `mods/Aurie` directory of the target game
  - To create a mod that uses the callback manager, include `CallbackManagerInterface.h` in the project and use the CallbackManagerInterface
    - Note: The header only provides a declaration of the CallbackManagerInterface struct and the available functions. Do not attempt to define any of the functions. The function definition will be found at runtime from `CallbackManagerMod.dll`.
