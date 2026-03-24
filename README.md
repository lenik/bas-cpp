# bas-cpp

Modern C++ library and small utilities built on top of **libbas-c** and related C/system libraries.  
It provides:

- **Unified byte/character I/O** over local files, URLs, and in‑memory buffers.
- **Volume abstraction** for treating different storage backends as virtual filesystems.
- **Formatting primitives** for JSON/text/binary “forms” over `VolumeFile`.
- **Optional scripting integration** for exposing data and operations to higher-level code.

This repository contains both the reusable library (`libbas-cpp`) and a set of small command‑line / GUI programs living in `app/`.

---

## Getting started

### Build

```bash
meson setup build
meson compile -C build
```

### Run tests (optional)

Tests are discovered under `src/**/**/*_test.cpp` and built automatically by Meson.  
They require the external `includes` helper that prints implementation sources for a given test.

```bash
meson test -C build
```

### Install

This installs the shared library, applications, and manpages to the configured prefix (e.g. `/usr/local`):

```bash
meson install -C build
```

Installed payload:

- **Library**: `libbas-cpp.so` → `libdir`
- **Applications**: each `app/*.cpp` → one executable in `bindir`
- **Man pages**: `app/*.1` → `mandir/man1`

---

## Dependencies

`bas-cpp` is a thin C++ layer over existing C and system libraries:

| Dependency      | Purpose |
|----------------|---------|
| **bas-c**      | Core C library and scriptable stack (GList) |
| **glib-2.0**   | Used by bas-c / scriptable layer |
| **icu-uc**     | Charset conversion for streams/readers/writers |
| **libcurl**    | HTTP/HTTPS/FTP/FTPS via `CurlInputStream` / `CurlOutputStream` |
| **openssl**    | TLS and crypto for HTTPS/FTPS |
| **boost**      | Locale, system, (optionally) core/assert/json |
| **zlib**       | ZIP/deflate support in volume layer |
| **ext2fs + com_err** *(optional)* | Userspace ext2/3/4 image access (`Ext4Volume`) |

Meson will detect these at configure time and fail fast if anything critical is missing.

---

## Library overview

The public C++ headers live under `src/` and are included relative to that root, e.g.:

```cpp
#include "io/InputStream.hpp"
#include "net/URL.hpp"
#include "volume/Volume.hpp"
```

Link against `libbas-cpp` and reuse the same dependencies as this project.

### I/O (`src/io/`)

- **InputSource / OutputTarget**: Abstract sources and sinks identified by URI/URL.  
  Provide `newInputStream()`, `newReader()`, `newOutputStream()`, `newWriter()` with optional charset.
- **InputStream / OutputStream**: Byte streams, with `RandomInputStream` for seekable input.
- **Reader / Writer / RandomReader**: Unicode character streams with line/char‑level APIs.
- **CharsetDecoder / CharsetEncoder**: ICU‑based conversion between encodings.
- **Helpers**: `LAInputStream`, `LAReader`, `Position`, `ISeekable`, and `IOException`.

Common pattern:

```cpp
std::unique_ptr<InputSource> src = /* e.g. URLInputSource or LocalInputSource */;
auto reader = src->newReader("UTF-8");
auto line = reader->readLine();
```

### Network and URL (`src/net/`)

- **URI / URL**: Parsing and manipulating URLs.
- **ProtocolRegistry**: Registry from scheme → `NetProtocol` implementation.
- **NetProtocol** + built‑ins:
  - `LocalProtocol` (file:),
  - `HttpProtocol` / `HttpsProtocol`,
  - `FtpProtocol` / `FtpsProtocol`,
  - `DataProtocol` (data: URIs).
- **CurlInputStream / CurlOutputStream**: libcurl‑based byte streams.
- **URLInputSource / URLOutputTarget**: `InputSource`/`OutputTarget` over URLs using `ProtocolRegistry`.

You can register custom protocols:

```cpp
ProtocolRegistry::instance().registerProtocol("myscheme", std::move(proto));
```

### Volumes (`src/volume/`)

The volume layer abstracts over storage backends (local dirs, ZIPs, etc.).

- **Volume**: Core interface: `getRootFile()`, `resolve(path)`, `getLocalFile(path)`, `getUUID()`, `getLabel()`, etc.  
  Paths are normalized (leading `/`, no trailing slash, resolves `.` / `..`).
- **VolumeFile**: Handle to a file/directory on a volume:
  - open as `InputStream`/`OutputStream`/`Reader`/`Writer`
  - list children
  - query type and status (`FileStatus`).
- **LocalVolume** (`src/volume/LocalVolume.*`): `Volume` backed by a local filesystem directory.
- **MemoryZip** (`src/volume/zip/MemoryZip.*`): In‑memory ZIP volume.
- **Fat32Volume** (`src/volume/fat32/`): FAT32 image-backed volume.
  - Uses dedicated file streams (`Fat32FileInputStream`, `Fat32FileOutputStream`) to avoid
    buffering the entire image in memory.
- **Ext4Volume** (`src/volume/ext4/`): ext2/ext3/ext4 image-backed volume.
  - Uses userspace `libext2fs` when available.
  - Resolves path->inode lazily and caches directory indexes by inode (no full-tree scan at mount).
  - Supports execution context (`uid/gid/groups`) and mode-based access checks.
- **VolumeManager**: Keeps a set of volumes and supports type-based lookup, URI resolution,
  and image volume registration (`addFat32Volume`, `addExt4Volume`).

Typical usage:

```cpp
LocalVolume vol("/some/root");
auto file = vol.resolve("/path/to/file.txt");
std::string data = file->readFileString("UTF-8");
```

Resolve from manager URI:

```cpp
VolumeManager vm;
vm.addLocalVolumes();
auto vf = vm.resolveUri("vol://0/etc/hosts");
if (vf) {
    auto text = vf->readFileString();
}
```

Mount image volumes:

```cpp
VolumeManager vm;
vm.addFat32Volume("/path/to/disk.fat32.img");
vm.addExt4Volume("/path/to/disk.ext4.img");
```

Notes:

- `Ext4Volume` requires `libext2fs` development headers/libs at build time.
- When ext2fs is not available, build still succeeds, but ext image operations fail with a clear exception.

### Formatting (`src/fmt/`)

File‑format helpers built around `VolumeFile`:

- **IFileForm / ITextForm**: Save/load logical objects to/from volume files.
- **IJsonForm / JsonFormOptions**: JSON forms using Boost.JSON (or vendored JSON).
- **IHtmlForm / IOctetStreamForm / IFileUtility**: HTML and raw binary helpers.

Implement these interfaces when you want reusable, testable persistence helpers instead of ad‑hoc serialization.

### Scripting (`src/script/`)

Optional layer for exposing library objects to a script engine:

- **IScriptSymbols**: Symbol table interface (`findVars`, `hasVar`, `formatVar`, `parseVar`, `findMethods`, `invokeMethod`, …).
- **ScriptApp**: Application‑level symbol stack, dispatching lookups across nested symbol providers.
- **json**: JSON helpers for scriptable data.

### Utilities and misc

- **Path / PathHelper** (`src/util/`): Path manipulation helpers.
- **LogBuffer**, **FileTypeDetector**, **ClipboardHelper**: Small focused utilities.
- **ImageFormat / Assets** (`src/proc/`): Helpers for dealing with images and app assets.
- **AccessException** (`src/security/`): Access‑denied style errors.

---

## Applications (`app/`)

Every `app/*.cpp` is built as a separate executable (discovered with `find app -maxdepth 1 -name '*.cpp'`), linked with `libbas-cpp`, and installed to `bindir`. Matching manpages `app/*.1` are installed to `mandir/man1`.

Current utilities include:

- **vols** – Lists local volumes discovered via `VolumeManager::addLocalVolumes()`, printing mountpoint, device, type, label, and UUID.

---

## License

`bas-cpp` is licensed under **AGPL‑3.0‑or‑later**, with an additional anti‑AI‑training term.  
See `debian/copyright` for full text.
