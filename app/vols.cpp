#include "bas/volume/LocalVolume.hpp"
#include "bas/volume/VolumeManager.hpp"

#include <bas/locale/i18n.h>
#include <bas/log/logger.h>
#include <bas/log/uselog.h>

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
    FUuid = 1u << 3,
    FLabel = 1u << 4,
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
    out << _("usage: vols [options]") << std::endl
        << "  " << _("Field selection (default without any: -mdtlu; with -v: -mdtlurLD):") << std::endl
        << "    -a, --all            " << _("all columns") << std::endl
        << "    -c, --compact        " << _("same columns as -mdtlu (merges with explicit field flags)") << std::endl
        << "    -m, --mountpoint     " << _("mount point column") << std::endl
        << "    -d, --device         " << _("device column") << std::endl
        << "    -t, --type           " << _("type column") << std::endl
        << "    -u, --uuid           " << _("UUID column") << std::endl
        << "    -l, --label          " << _("label column") << std::endl
        << "    -r, --read-only      " << _("read-only (ro/rw) column") << std::endl
        << "    -L, --logical-type   " << _("logical type column") << std::endl
        << "    -D, --loop-device    " << _("loop device column") << std::endl
        << "    -o, --opts           " << _("per-mount (VFS) options from mountinfo") << std::endl
        << "    -O, --superopts      " << _("superblock options from mountinfo") << std::endl
        << "    -z, --root           " << _("root path within filesystem (mountinfo field)") << std::endl
        << "  " << _("Other:") << std::endl
        << "    -s, --symbols        " << _("include overlay filesystems in the scan (bind mounts always on)") << std::endl
        << "        --loops          " << _("include loopback mounts (excluded by default)") << std::endl
        << "    -h, --help           " << _("show this help and exit") << std::endl
        << "    -w, --writable       " << _("list only writable mounts; implies -r") << std::endl
        << "    -v, --verbose        " << _("more log detail; default columns include logical type") << std::endl
        << "    -q, --quiet          " << _("only errors on stderr") << std::endl;
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
    case FUuid:
        return local->getUuid();
    case FLabel:
        return local->getLabel();
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
    {FMount, N_("Mount Point")},
    {FRoot, N_("Root")},
    {FDevice, N_("Device")},
    {FType, N_("Type")},
    {FLabel, N_("Label")},
    {FUuid, N_("UUID")},
    {FReadOnly, N_("Read-Only")},
    {FLogical, N_("Logical Type")},
    {FLoop, N_("Loop Device")},
    {FVfsOpts, N_("VFS opts")},
    {FSuperOpts, N_("Super opts")},
}};

} // namespace

int main(int argc, char** argv) {
    init_i18n(LOCALEDIR);

    bool wantHelp = false;
    bool verbose = false;
    bool quiet = false;
    bool excludeReadOnly = false;
    bool writableOpt = false;
    bool includeSymbols = false;
    bool includeLoops = false;
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
        {"loops", no_argument, nullptr, 1000},
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
        case 1000:
            includeLoops = true;
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
        std::cerr << _("vols: unexpected argument: ") << argv[optind] << '\n';
        printUsage(std::cerr);
        return 2;
    }

    if (wantHelp) {
        printUsage(std::cout);
        return 0;
    }

    if (verbose && quiet) {
        std::cerr << _("vols: -v/--verbose and -q/--quiet are mutually exclusive\n");
        return 2;
    }

    if (allOpt && compactOpt) {
        std::cerr << _("vols: -a/--all and -c/--compact are mutually exclusive\n");
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
    vm.addLocalVolumes(includeSymbols || (fields & FLogical) != 0, excludeReadOnly, !includeLoops);

    const auto& vols = vm.all();

    std::vector<Field> active;
    active.reserve(kColumnOrder.size());
    for (const ColDef& c : kColumnOrder) {
        if (fields & c.bit) {
            active.push_back(c.bit);
        }
    }

    if (active.empty()) {
        std::cerr << _("vols: no columns selected\n");
        return 2;
    }

    std::vector<size_t> widths(active.size());
    for (size_t i = 0; i < active.size(); ++i) {
        for (const ColDef& c : kColumnOrder) {
            if (c.bit == active[i]) {
                widths[i] = std::char_traits<char>::length(_(c.title));
                break;
            }
        }
    }

    for (const auto& vol : vols) {
        auto* local = dynamic_cast<LocalVolume*>(vol.get());
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
                std::cout << std::setw(static_cast<int>(widths[i])) << std::left << _(c.title);
                break;
            }
        }
    }
    std::cout << '\n';

    for (const auto& vol : vols) {
        auto* local = dynamic_cast<LocalVolume*>(vol.get());
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
