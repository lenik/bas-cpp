# OmniShell registry (`src/core/registry`)

This directory implements a **typed, hierarchical configuration store** behind a common interface (`IRegistry`). Values are keyed by dot-separated paths (e.g. `Desktop.Background.Mode`). The same logical model is backed by different storage implementations: local filesystem tree, a single JSON file, or a **Volume** (VFS) tree.

The design goals are:

- **One API** for shell code and modules: read/write primitives, tree listing, optional change notifications.
- **Stable on-disk layout** for the default (`LocalRegistry`) with a one-time migration from an older flat file.
- **Portable value encoding** as strings inside JSON string literals on disk, while using rich C++ types in memory (including Howard Hinnant’s **date** library via `libdate-tz`).

---

## Architecture overview

```
                    ┌─────────────────┐
                    │    IRegistry    │
                    │  (abstract API) │
                    └────────┬────────┘
                             │
         ┌───────────────────┼───────────────────┐
         │                   │                   │
         ▼                   ▼                   ▼
┌────────────────┐  ┌────────────────┐  ┌────────────────┐
│ LocalRegistry  │  │  JsonRegistry  │  │ VolumeRegistry │
│ ~/.config/...  │  │  one .json     │  │ VolumeFile     │
└────────────────┘  └────────────────┘  └────────────────┘
```

- **`IRegistry`** — abstract base; default helpers (`getString`, `getBool`, `set`, datetime accessors, `watch` / `unwatch`) are implemented in terms of `getVariant` / `setVariant`.
- **`LocalRegistry`** — canonical desktop store: directory tree under `~/.config/<appname>/registry/`.
- **`JsonRegistry`** — optional backend: one file containing a flat map of string keys to string values (legacy-style).
- **`VolumeRegistry`** — same **logical** layout as `LocalRegistry`, but all I/O goes through **`VolumeFile`** (no direct `std::filesystem` for the tree).

**Entry points for the running shell**

- **`os::registry()`** (`RegistryService.hpp` / `.cpp`) returns **`IRegistry&`** → **`LocalRegistry::instance()`**.
- **`RegistryDb::getInstance()`** (`core/RegistryDb.hpp`) is a thin compatibility alias for **`registry()`**.

---

## Key types (`RegistryTypes.hpp`)

All registry “scalar” types live in namespace **`os::reg`**.

### `registry_value_t`

A **`std::variant`** holding exactly one of:

| Branch | C++ type | Role |
|--------|----------|------|
| Boolean | `bool` | `true` / `false` |
| Integer | `int`, `long` | Whole numbers (parsing distinguishes by range / literal) |
| Floating | `float`, `double` | Floating point |
| Text | `std::string` | Opaque or human-readable text |
| UTC instant | `reg::sys_time` → `date::sys_seconds` | System-clock UTC, second precision |
| Local instant | `reg::local_time` → `date::local_seconds` | `local_t` timeline (see date library) |
| Zoned instant | `reg::zoned_time` → `date::zoned_seconds` | IANA zone + UTC instant; needs **tz database** |
| Calendar date | `reg::year_month_day` → `date::year_month_day` | Calendar date (not a time-of-day) |
| Clock time | `reg::time_of_day` → `date::hh_mm_ss<std::chrono::seconds>` | Time-of-day within a day |

Headers pull in **`<date/tz.h>`** so `zoned_seconds` is well-defined; linking requires **`libdate-tz`** (see project `meson.build`).

### `variant_t`

```cpp
using variant_t = std::optional<registry_value_t>;
```

- **`std::nullopt`** means “no value” / removal semantics when passed to **`setVariant`**.
- Otherwise the optional holds one **`registry_value_t`**.

---

## On-disk string encoding (`RegistryValueIo`)

Regardless of backend, each **leaf** value is ultimately stored as a **string**. That string is the serialization of a **`registry_value_t`**, using deterministic prefixes so types round-trip where possible.

| Prefix / form | Type | Example |
|---------------|------|---------|
| `true` / `false` | `bool` | `true` |
| Integer literal | `int` or `long` | `42`, large → `long` |
| Floating literal | `float` or `double` | `3.14` |
| `sys:` | `sys_time` | `sys:1700000000` |
| `local:` | `local_time` | `local:1700000000` |
| `zoned:` | `zoned_time` | `zoned:Europe/Berlin:1700000000` |
| `ymd:` | `year_month_day` | `ymd:2024-3-20` |
| `tod:` | `time_of_day` | `tod:14:30:0` |
| (none) | `std::string` | Any text that does not parse as the above |

**Important details**

- **Plain integers** are **`int`/`long`**, not UTC times. Use the **`sys:`** prefix when you need a **`sys_time`** that survives load/save.
- **`zoned:`** parsing uses **`date::locate_zone`**. Invalid zone names or missing tz data cause the raw string to be stored as a **`std::string`** branch instead of failing the whole registry load.
- **`tryInt` / `tryLong` / …** (strict parsing for optional getters) require the **entire** string to parse as a number; **`optionalInt`** (defaulting getters) is more lenient.

The physical container for that string is described below per backend.

---

## `LocalRegistry` (default)

### Layout

Root directory:

```text
$HOME/.config/<wxAppName>/registry/
```

- **Key** `A.B.C` maps to file path **`A/B/C.json`** (directories = namespace segments).
- Each **`.json` file** contains a **single JSON string value** (JSON string literal), whose **content** is the encoded value from the table above (e.g. `"\"true\""` on disk for boolean true after escaping).

### Lazy load and migration

- On first use, **`ensureLoaded()`** runs **`loadFromDiskOrMigrate()`**.
- If **`registry.json`** exists in **`~/.config/<app>/`** and the **`registry/`** tree is empty, the flat file is parsed, written out as the tree, and the legacy file is renamed to **`registry.json.bak`** on success.
- If the tree is empty after load, a small set of **default keys** is seeded (`System.OS.*`, `User.*`) and written.

### Persistence behaviour

- **`remove`** / **`delTree`** / **`setVariant`** to clear a key update the in-memory map and delete or adjust files where applicable.
- **`save()`** flushes the **entire** map to files (full rewrite of all known keys).
- Some **`set`** paths update memory and emit **`emitChanged`** without immediately writing every leaf; callers that need durability should call **`save()`** after batches (existing shell code often does).

---

## `JsonRegistry`

- Construct with a **filesystem path** to one JSON file.
- The file is treated as a **flat** object: keys are full dotted names, values are strings (same encoding as above).
- Useful for tests, import/export, or tooling; not the default for the shell.

---

## `VolumeRegistry`

- Construct with a **`VolumeFile`** pointing at a **directory** that will act as the registry root.
- Same mapping as **`LocalRegistry`**: `A.B.C` → **`A/B/C.json`** under that root, each file body = JSON string literal holding the encoded value.
- If the tree is empty after load, the same **default seed** as **`LocalRegistry`** is applied and written via volume I/O.
- **`save()`** writes all entries under the root using **`VolumeFile`** APIs (`resolve`, `readDir`, `writeFileString`, `createParentDirectories`, `removeFile`, etc.).

---

## `IRegistry` API (behavioural notes)

### Core virtual methods

| Method | Purpose |
|--------|---------|
| `load()` | Reload from storage (implementations may clear and rescan). |
| `save() const` | Persist current state. |
| `list(node_key, full_key)` | List child names under a logical node. `node_key == ""` → roots. `full_key == true` → dotted full names; `false` → last segment only. |
| `getVariant` / `setVariant` | Low-level typed access. |
| `has` | Key exists. |
| `remove` | Delete one leaf. |
| `delTree` | Delete the node and every key whose name equals or extends `node_key` with a dot. |
| `snapshotStrings()` | Copy of all keys → encoded string values (for UI like the Registry editor). |

### Convenience accessors (non-virtual, in `IRegistry.cpp`)

- **`getString`** — avoids overload ambiguity with **`get(key, registry_value_t)`**; always uses string coercion for display/storage.
- **`get` / `getBool` / …** — two styles:
  - **`std::optional<T>`** with optional fallback: missing key → fallback; wrong type → `nullopt` for **`try*`**-based paths.
  - **`T` with default**: missing or uncoercible uses the provided default (lenient **`optional*`** helpers for numerics).

Datetime mirrors the same pattern: **`getSysTime`**, **`getLocalTime`**, **`getZonedTime`**, **`getYearMonthDay`**, **`getTimeOfDay`**, plus **`set`** overloads.

### Change notifications (Boost.Signals2)

- **`watch(prefix, slot)`** returns **`boost::signals2::connection`** (typedef **`watch_handle`**).
- **`unwatch(connection)`** calls **`disconnect()`**.
- A slot runs when the changed **key** matches:
  - **`prefix` empty** → all keys, or
  - **`key == prefix`**, or
  - **`key`** starts with **`prefix + "."`**
- **`emitChanged(key, new_value, old_value)`** is invoked from concrete implementations when mutating.

Implementations should call **`emitChanged`** after updating internal maps so watchers stay consistent.

---

## File type associations

Structured **open-with** handlers and **icons** for extensions, MIME types, folders, and path/filename rules are specified in **[FILE_ASSOCIATIONS.md](FILE_ASSOCIATIONS.md)**. Keys live under **`OmniShell.FileTypes.*`**; each leaf value is a registry string holding JSON (see that doc for `FileType`, `ImageSet`, and array vs map shapes).

---

## Supporting files

| File | Role |
|------|------|
| `FILE_ASSOCIATIONS.md` | Schema for **file-types** in the registry (`extension`, `mime-type`, `folder`, path/glob arrays, `openwith`, icons). |
| `RegistryValueIo.hpp` / `.cpp` | Serialize / parse **`registry_value_t`** ↔ string; **`try*`** helpers. |
| `RegistryKeyUtil.hpp` / `.cpp` | **`listChildKeys`**, **`keysForDelTree`** over the flat `std::map` representation. |
| `RegistryService.hpp` / `.cpp` | **`os::registry()`** → **`LocalRegistry`**. |
| `../RegistryDb.hpp` | Legacy **`RegistryDb::getInstance()`** shim. |

---

## Build and dependencies

- **Boost** — **Boost.Signals2** (header-heavy; pulled via Meson `dependency('boost')`).
- **Howard Hinnant date** — headers + **`libdate-tz`** for **`zoned_time`** and tz operations.
- **Consumers** of installed headers that include **`IRegistry.hpp`** / **`RegistryTypes.hpp`** need compatible Boost and date dev packages (see **`debian/control`**).

---

## Usage snippets

```cpp
#include "core/registry/RegistryService.hpp"

os::IRegistry& r = os::registry();

r.set("MyModule.Caption", std::string{"Hello"});
std::string cap = r.getString("MyModule.Caption", "");
r.set("MyModule.Enabled", true);
r.save();

auto h = r.watch("MyModule", [](const std::string& key,
                                const os::reg::variant_t& nv,
                                const os::reg::variant_t& ov) {
    (void)key;
    (void)nv;
    (void)ov;
    // e.g. refresh UI when MyModule or MyModule.* changes
});
// r.unwatch(h);  // or h.disconnect();
```

```cpp
// Explicit backend (not the global singleton)
os::LocalRegistry& local = os::LocalRegistry::instance();
os::JsonRegistry json{"/path/to/registry.json"};
os::VolumeRegistry vol{volumeFileToRegistryRoot};
```

---

## File index (this directory)

| Source | Description |
|--------|-------------|
| `FILE_ASSOCIATIONS.md` | File type / open-with / icon registry schema. |
| `IRegistry.hpp` / `.cpp` | Abstract interface + default `get`/`set`/`watch` implementation. |
| `RegistryTypes.hpp` | `registry_value_t`, `variant_t`, date typedefs. |
| `RegistryValueIo.hpp` / `.cpp` | Value ↔ string encoding and `try*` / `optional*` helpers. |
| `RegistryKeyUtil.hpp` / `.cpp` | Key tree helpers for `list` / `delTree`. |
| `LocalRegistry.hpp` / `.cpp` | Default filesystem backend + migration. |
| `JsonRegistry.hpp` / `.cpp` | Single-file JSON backend. |
| `VolumeRegistry.hpp` / `.cpp` | Volume-backed tree backend. |
| `RegistryService.hpp` / `.cpp` | Global `registry()` accessor. |

---

## Further reading

- Howard Hinnant **date** / **tz**: packaged on Debian as **`libhowardhinnant-date-dev`**; headers under **`/usr/include/date/`**.
- **Volume** API: **`bas/volume/VolumeFile.hpp`** (resolve, read/write string, directory operations).

If you add a new **`IRegistry`** implementation, ensure **`getVariant`**, **`setVariant`**, **`list`**, **`has`**, **`remove`**, **`delTree`**, **`load`**, **`save`**, and **`snapshotStrings`** remain consistent with the encoding rules above, and call **`emitChanged`** whenever a key’s logical value changes.
