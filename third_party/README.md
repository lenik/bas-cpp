# Third-party dependencies

Boost libraries are full git clones from [boostorg](https://github.com/boostorg) and are aligned with **Boost 1.67** where the library existed in that release.

## Version alignment (Boost 1.67)

| Library       | Tag / version   | Notes |
|---------------|-----------------|--------|
| **boost_core**   | `boost-1.67.0`  | Aligned with Boost 1.67. |
| **boost_system** | `boost-1.67.0`  | Aligned with Boost 1.67. |
| **boost_json**   | `boost-1.75.0`  | Not in 1.67; first release 1.75. |
| **boost_assert** | `boost-1.67.0`  | Full repo; aligned with Boost 1.67. |

Libraries **boost_variant2**, **boost_compat**, **boost_describe** are present as full repos but are not on the include path when using the above versions (JSON 1.75.0 and System 1.67.0 do not use them). They are kept for reference or future use.

## Updating

From the repo root:

```bash
cd third_party/boost_<name>
git fetch --tags
git checkout boost-1.67.0   # or desired tag
```

## Repo URLs

- https://github.com/boostorg/core → `boost_core`
- https://github.com/boostorg/system → `boost_system`
- https://github.com/boostorg/json → `boost_json`
- https://github.com/boostorg/assert → `boost_assert`
- https://github.com/boostorg/variant2 → `boost_variant2`
- https://github.com/boostorg/compat → `boost_compat`
- https://github.com/boostorg/describe → `boost_describe`
