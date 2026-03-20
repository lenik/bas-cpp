#include "volume/LocalVolume.hpp"
#include "volume/VolumeManager.hpp"

#include <log/logger.h>
#include <log/uselog.h>

#include <getopt.h>

#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

enum Field : unsigned {
    FMount = 1u << 0,
    FDevice = 1u << 1,
    FType = 1u << 2,
    FLabel = 1u << 3,
    FUuid = 1u << 4,
    FReadOnly = 1u << 5,
    FLogical = 1u << 6,
    FLoop = 1u << 7,
    FVfsOpts = 1u << 8,
    FSuperOpts = 1u << 9,
    FRoot = 1u << 10,
};

constexpr unsigned kDefaultFields = FMount | FDevice | FType | FLabel | FUuid;
constexpr unsigned kVerboseDefaultFields = kDefaultFields | FReadOnly | FLogical | FLoop;
constexpr unsigned kAllFields = FMount | FRoot | FDevice | FType | FLabel | FUuid | FReadOnly | FLogical | FLoop
    | FVfsOpts | FSuperOpts;

void printUsage(std::ostream& out) {
    out << "usage: vols [options]\n"
           "  Field selection (default without any: -mdtlu; with -v: -mdtlurLD):\n"
           "    -a, --all            all columns\n"
           "    -c, --compact        same columns as -mdtlu (merges with explicit field flags)\n"
           "    -m, --mountpoint     mount point column\n"
           "    -d, --device         device column\n"
           "    -t, --type           type column\n"
           "    -l, --label          label column\n"
           "    -u, --uuid           UUID column\n"
           "    -r, --read-only      read-only (ro/rw) column\n"
           "    -L, --logical-type   logical type column\n"
           "    -D, --loop-device    loop device column\n"
           "    -o, --opts           per-mount (VFS) options from mountinfo\n"
           "    -O, --superopts      superblock options from mountinfo\n"
           "    -z, --root           root path within filesystem (mountinfo field)\n"
           "  Other:\n"
           "    -s, --symbols        include overlay filesystems in the scan (bind mounts always on)\n"
           "    -h, --help           show this help and exit\n"
           "    -w, --writable       list only writable mounts; implies -r\n"
           "    -v, --verbose        more log detail; default columns include logical type\n"
           "    -q, --quiet          only errors on stderr\n";
}

const char* logicalTypeLabel(LocalLogicalType t) {
    switch (t) {
    case LocalLogicalType::BIND:
        return "bind";
    case LocalLogicalType::OVERLAY:
        return "overlay";
    case LocalLogicalType::NONE:
    default:
        return "-";
    }
}

std::string fieldCell(LocalVolume* local, Field bit) {
    switch (bit) {
    case FMount: {
        const auto& o = local->getMountPoint();
        return o ? *o : std::string(local->getRootPath());
    }
    case FRoot: {
        std::string v = local->getMountInfo().root;
        return v.empty() ? std::string("-") : v;
    }
    case FDevice: {
        const auto& o = local->getDevice();
        return o ? *o : std::string("-");
    }
    case FType:
        return local->getTypeString();
    case FLabel:
        return local->getLabel();
    case FUuid:
        return local->getUUID();
    case FReadOnly:
        return local->isReadOnly() ? "ro" : "rw";
    case FLogical:
        return logicalTypeLabel(local->getLogicalType());
    case FLoop:
        return local->isLoop() ? "loop" : "-";
    case FVfsOpts: {
        std::string v = local->getMountInfo().vfsOpts;
        return v.empty() ? std::string("-") : v;
    }
    case FSuperOpts: {
        std::string v = local->getMountInfo().superOpts;
        return v.empty() ? std::string("-") : v;
    }
    default:
        return {};
    }
}

struct ColDef {
    Field bit;
    const char* title;
};

constexpr std::array<ColDef, 11> kColumnOrder = {{
    {FMount, "Mount Point"},
    {FRoot, "Root"},
    {FDevice, "Device"},
    {FType, "Type"},
    {FLabel, "Label"},
    {FUuid, "UUID"},
    {FReadOnly, "Read-Only"},
    {FLogical, "Logical Type"},
    {FLoop, "Loop Device"},
    {FVfsOpts, "VFS opts"},
    {FSuperOpts, "Super opts"},
}};

} // namespace

int main(int argc, char** argv) {
    bool wantHelp = false;
    bool verbose = false;
    bool quiet = false;
    bool excludeReadOnly = false;
    bool writableOpt = false;
    bool includeSymbols = false;
    bool anyFieldOption = false;
    bool allOpt = false;
    bool compactOpt = false;
    unsigned fields = 0;

    static const struct option long_opts[] = {
        {"help", no_argument, nullptr, 'h'},
        {"all", no_argument, nullptr, 'a'},
        {"compact", no_argument, nullptr, 'c'},
        {"mountpoint", no_argument, nullptr, 'm'},
        {"device", no_argument, nullptr, 'd'},
        {"type", no_argument, nullptr, 't'},
        {"label", no_argument, nullptr, 'l'},
        {"uuid", no_argument, nullptr, 'u'},
        {"read-only", no_argument, nullptr, 'r'},
        {"logical-type", no_argument, nullptr, 'L'},
        {"loop-device", no_argument, nullptr, 'D'},
        {"opts", no_argument, nullptr, 'o'},
        {"superopts", no_argument, nullptr, 'O'},
        {"root", no_argument, nullptr, 'z'},
        {"symbols", no_argument, nullptr, 's'},
        {"writable", no_argument, nullptr, 'w'},
        {"verbose", no_argument, nullptr, 'v'},
        {"quiet", no_argument, nullptr, 'q'},
        {nullptr, 0, nullptr, 0},
    };

    int opt;
    int longIndex = 0;
    while ((opt = getopt_long(argc, argv, "achmdtluLrsDoOvwqz", long_opts, &longIndex)) != -1) {
        switch (opt) {
        case 'h':
            wantHelp = true;
            break;
        case 'a':
            allOpt = true;
            break;
        case 'c':
            compactOpt = true;
            break;
        case 'm':
            fields |= FMount;
            anyFieldOption = true;
            break;
        case 'd':
            fields |= FDevice;
            anyFieldOption = true;
            break;
        case 't':
            fields |= FType;
            anyFieldOption = true;
            break;
        case 'l':
            fields |= FLabel;
            anyFieldOption = true;
            break;
        case 'u':
            fields |= FUuid;
            anyFieldOption = true;
            break;
        case 'r':
            fields |= FReadOnly;
            anyFieldOption = true;
            break;
        case 'L':
            fields |= FLogical;
            anyFieldOption = true;
            break;
        case 'D':
            fields |= FLoop;
            anyFieldOption = true;
            break;
        case 'o':
            fields |= FVfsOpts;
            anyFieldOption = true;
            break;
        case 'O':
            fields |= FSuperOpts;
            anyFieldOption = true;
            break;
        case 'z':
            fields |= FRoot;
            anyFieldOption = true;
            break;
        case 's':
            includeSymbols = true;
            break;
        case 'w':
            excludeReadOnly = true;
            writableOpt = true;
            break;
        case 'v':
            verbose = true;
            break;
        case 'q':
            quiet = true;
            break;
        default:
            printUsage(std::cerr);
            return 2;
        }
    }

    if (optind < argc) {
        std::cerr << "vols: unexpected argument: " << argv[optind] << '\n';
        printUsage(std::cerr);
        return 2;
    }

    if (wantHelp) {
        printUsage(std::cout);
        return 0;
    }

    if (verbose && quiet) {
        std::cerr << "vols: -v/--verbose and -q/--quiet are mutually exclusive\n";
        return 2;
    }

    if (allOpt && compactOpt) {
        std::cerr << "vols: -a/--all and -c/--compact are mutually exclusive\n";
        return 2;
    }

    if (allOpt) {
        fields = kAllFields;
    } else if (!anyFieldOption) {
        if (compactOpt) {
            fields = kDefaultFields;
        } else {
            fields = verbose ? kVerboseDefaultFields : kDefaultFields;
        }
    } else if (compactOpt) {
        fields = kDefaultFields | fields;
    }

    if (writableOpt) {
        fields |= FReadOnly;
    }

    if (verbose) {
        set_loglevel(LOG_LEVEL_DEBUG);
    } else if (quiet) {
        set_loglevel(LOG_LEVEL_ERROR);
    }

    VolumeManager vm;
    vm.addLocalVolumes(includeSymbols || (fields & FLogical) != 0, excludeReadOnly);

    const auto& vols = vm.all();

    std::vector<Field> active;
    active.reserve(kColumnOrder.size());
    for (const ColDef& c : kColumnOrder) {
        if (fields & c.bit) {
            active.push_back(c.bit);
        }
    }

    if (active.empty()) {
        std::cerr << "vols: no columns selected\n";
        return 2;
    }

    std::vector<size_t> widths(active.size());
    for (size_t i = 0; i < active.size(); ++i) {
        for (const ColDef& c : kColumnOrder) {
            if (c.bit == active[i]) {
                widths[i] = std::char_traits<char>::length(c.title);
                break;
            }
        }
    }

    for (const auto& uptr : vols) {
        auto* local = dynamic_cast<LocalVolume*>(uptr.get());
        if (!local) {
            continue;
        }
        for (size_t i = 0; i < active.size(); ++i) {
            std::string cell = fieldCell(local, active[i]);
            widths[i] = std::max(widths[i], cell.size());
        }
    }

    for (size_t i = 0; i < active.size(); ++i) {
        for (const ColDef& c : kColumnOrder) {
            if (c.bit == active[i]) {
                if (i) {
                    std::cout << '\t';
                }
                std::cout << std::setw(static_cast<int>(widths[i])) << std::left << c.title;
                break;
            }
        }
    }
    std::cout << '\n';

    for (const auto& uptr : vols) {
        auto* local = dynamic_cast<LocalVolume*>(uptr.get());
        if (!local) {
            continue;
        }
        for (size_t i = 0; i < active.size(); ++i) {
            if (i) {
                std::cout << '\t';
            }
            std::cout << std::setw(static_cast<int>(widths[i])) << std::left << fieldCell(local, active[i]);
        }
        std::cout << '\n';
    }

    return 0;
}
