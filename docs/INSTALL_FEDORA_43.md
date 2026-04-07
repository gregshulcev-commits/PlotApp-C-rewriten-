# Installation on Fedora 43

## Packages

```bash
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel minizip-ng-compat-devel
```

## Configure and build

```bash
cmake -S . -B build -GNinja
cmake --build build
```

## Run tests

```bash
ctest --test-dir build --output-on-failure
```

## Run the desktop app

```bash
./build/plotapp
```

## Run the CLI

```bash
./build/plotapp-cli
```

## Optional custom plugin path

When needed, plugins can also be discovered from an explicit directory list:

```bash
PLOTAPP_PLUGIN_DIR=/path/to/plugins ./build/plotapp-cli plugins
```

Multiple directories can be provided with `:`.

## Notes

- The desktop GUI target is built only when Qt6 development files are available.
- The non-GUI core, plugins, CLI, and tests can still be built in environments without Qt by passing `-DPLOTAPP_BUILD_GUI=OFF`.
