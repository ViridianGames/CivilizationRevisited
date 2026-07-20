# Civilization Revisited

Based on [GeistStarterProject](../GeistStarterProject). Geist engine + raylib.

## Build

```bash
mkdir -p cmake-build-debug && cd cmake-build-debug
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

Binary lands in `Redist/` (debug name: `CivilizationRevisited_Debug`). Run from `Redist/` so `engine.cfg` and assets resolve.

## Controls

- **ESC** — quit
