#include "bas/reg/Registry.hpp"
#include <bas/fmt/JsonForm.hpp>
#include <bas/log/logger.h>
#include <bas/log/uselog.h>
#include <bas/reg/JsonRegistry.hpp>
#include <bas/reg/LocalRegistry.hpp>
#include <bas/reg/VolumeRegistry.hpp>
#include <bas/reg/variant.hpp>
#include <bas/volume/LocalVolume.hpp>
#include <bas/volume/VolumeFile.hpp>

#include <boost/json.hpp>

#include <getopt.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace bas::reg {

constexpr const char* kVersion = "1.0.1";

void printUsage(std::ostream& out) {
    out << "usage: reg [options]\n"
           "  -r, --root DIR|FILE   registry root (directory → LocalRegistry, file → "
           "JsonRegistry)\n"
           "  -d, --dump PATH       list all keys under PATH (path=value lines)\n"
           "  -g, --get PATH        print registry value (valueToString)\n"
           "  -s, --set PATH VALUE  set registry value (parseOption)\n"
           "  -e, --export PATH     export subtree at PATH as JSON to stdout\n"
           "  -i, --import PATH     read JSON from stdin and merge at PATH\n"
           "  -v, --verbose         debug logging\n"
           "  -q, --quiet           errors only on stderr\n"
           "      --version         print version and exit\n"
           "  -h, --help            show this help and exit\n";
}

std::string joinDir(std::string_view base, std::string_view name) {
    if (base.empty() || base == "/")
        return std::string(name);
    std::string out(base);
    if (out.back() != '/')
        out.push_back('/');
    out.append(name);
    return out;
}

std::string joinKey(std::string_view base, const std::string_view name) {
    if (base.empty() || base == "/" || base == ".")
        return std::string(name);
    std::string out(base);
    if (out.back() != '.' && out.back() != '/') {
        out.push_back('.');
    }
    out.append(name);
    return out;
}

void dumpTree(const IRegistry& reg, std::string_view path, NodeInfo info, std::ostream& out,
              std::string_view prefix) {
    auto children = reg.list(path);
    // if (children.empty()) {
    auto opt = reg.getOption(path);
    if (info.value) {
        out << " => " << optionToString(opt);
    } else {
        // out << "(none)";
    }

    std::vector<const regkey_t*> keys;
    keys.reserve(children.size());
    for (const auto& [key, info] : children) {
        keys.push_back(&key);
    }

    for (size_t i = 0; i < keys.size(); ++i) {
        const regkey_t& key = *keys[i];
        const auto& childInfo = children.at(key);

        bool last = i == keys.size() - 1;

        auto name = keyToString(key);
        auto j = info.directory ? joinDir(path, name) : joinKey(path, name);
        out << std::endl << prefix;

        if (last)
            out << " `- ";
        else
            out << " |- ";

        out << name;
        if (childInfo.directory)
            out << (childInfo.domain ? "▷" : "/");
        else if (childInfo.domain)
            out << ".";

        // out << " => ";
        auto next_prefix = std::string(prefix) + (last ? "    " : " |  ");
        dumpTree(reg, j, childInfo, out, next_prefix);
    }
}

boost::json::value exportNode(const IRegistry& reg, std::string_view path) {
    const auto children = reg.list(path);
    if (children.empty()) {
        auto opt = reg.getOption(path);
        if (!opt.has_value())
            return boost::json::value(nullptr);
        return optionToJson(opt);
    }

    boost::json::object obj;
    for (const auto& [child, info] : children) {
        auto name = keyToString(child);
        const std::string childPath = info.directory ? joinDir(path, name) : joinKey(path, name);
        obj.emplace(name, exportNode(reg, childPath));
    }
    return obj;
}

void importJson(IRegistry& reg, std::string_view path, const boost::json::value& jv) {
    if (jv.is_object()) {
        for (const auto& [key, value] : jv.as_object()) {
            importJson(reg, joinKey(path, std::string(key)), value);
        }
        return;
    }
    if (jv.is_array()) {
        const auto& arr = jv.as_array();
        for (size_t i = 0; i < arr.size(); ++i) {
            importJson(reg, joinKey(path, std::to_string(i)), arr[i]);
        }
        return;
    }
    reg.setOption(path, jsonToOption(jv));
}

struct RegistryContext {
    std::shared_ptr<LocalVolume> localVolume;
    std::shared_ptr<VolumeRegistry> volume;
    std::shared_ptr<JsonRegistry> json;
    IRegistry* reg = nullptr;
    IContainerManager* containers = nullptr;
    bool jsonDirty = false;

    void markDirty() { jsonDirty = true; }

    void flush() {
        if (containers) {
            containers->flushCachedContainers();
        } else if (jsonDirty && json) {
            json->save();
        }
    }
};

bool openRegistry(const std::string& rootOpt, RegistryContext& ctx) {
    if (rootOpt.empty()) {
        ctx.reg = &LocalRegistry::userDefault();
        ctx.containers = dynamic_cast<IContainerManager*>(ctx.reg);
        return true;
    }

    namespace fs = std::filesystem;
    const fs::path rootPath = fs::absolute(rootOpt);
    std::error_code ec;
    if (!fs::exists(rootPath, ec)) {
        if (rootPath.extension() == ".json") {
            std::ofstream create(rootPath);
            if (!create) {
                std::cerr << "reg: cannot create " << rootOpt << '\n';
                return false;
            }
            create << "{}";
        } else if (!fs::create_directories(rootPath, ec) || ec) {
            std::cerr << "reg: cannot create directory " << rootOpt << '\n';
            return false;
        }
    }
    if (fs::is_directory(rootPath, ec)) {
        ctx.localVolume = std::make_shared<LocalVolume>(rootPath.string());
        ctx.volume =
            std::make_shared<VolumeRegistry>(VolumeFile(ctx.localVolume, std::string("/")));
        ctx.reg = ctx.volume.get();
        ctx.containers = dynamic_cast<IContainerManager*>(ctx.reg);
        return true;
    }
    if (fs::is_regular_file(rootPath, ec)) {
        try {
            ctx.json = JsonRegistry::load(rootPath);
        } catch (const std::exception& e) {
            std::cerr << "reg: " << e.what() << '\n';
            return false;
        }
        ctx.reg = ctx.json.get();
        return true;
    }

    std::cerr << "reg: root is neither a directory nor a regular file: " << rootOpt << '\n';
    return false;
}

int main(int argc, char** argv) {
    bool wantHelp = false;
    bool wantVersion = false;
    bool verbose = false;
    bool quiet = false;

    std::string rootPath;
    std::optional<std::string> dumpPath;
    std::string getPath;
    std::string setPath;
    std::string setValue;
    std::string exportPath;
    std::string importPath;

    static const struct option long_opts[] = {
        {"help", no_argument, nullptr, 'h'},
        {"root", required_argument, nullptr, 'r'},
        {"dump", required_argument, nullptr, 'd'},
        {"get", required_argument, nullptr, 'g'},
        {"set", required_argument, nullptr, 's'},
        {"export", required_argument, nullptr, 'e'},
        {"import", required_argument, nullptr, 'i'},
        {"verbose", no_argument, nullptr, 'v'},
        {"quiet", no_argument, nullptr, 'q'},
        {"version", no_argument, nullptr, 0},
        {nullptr, 0, nullptr, 0},
    };

    int opt;
    int longIndex = 0;
    while ((opt = getopt_long(argc, argv, "hr:d:g:s:e:i:vq", long_opts, &longIndex)) != -1) {
        switch (opt) {
        case 'h':
            wantHelp = true;
            break;
        case 'r':
            rootPath = optarg;
            break;
        case 'd':
            dumpPath = optarg ? std::string(optarg) : std::string{};
            break;
        case 'g':
            getPath = optarg;
            break;
        case 's':
            setPath = optarg;
            if (optind >= argc) {
                std::cerr << "reg: --set requires a value argument\n";
                printUsage(std::cerr);
                return 2;
            }
            setValue = argv[optind++];
            break;
        case 'e':
            exportPath = optarg;
            break;
        case 'i':
            importPath = optarg;
            break;
        case 'v':
            verbose = true;
            break;
        case 'q':
            quiet = true;
            break;
        case 0:
            if (std::strcmp(long_opts[longIndex].name, "version") == 0) {
                wantVersion = true;
            }
            break;
        default:
            printUsage(std::cerr);
            return 2;
        }
    }

    if (optind < argc) {
        std::cerr << "reg: unexpected argument: " << argv[optind] << '\n';
        printUsage(std::cerr);
        return 2;
    }

    if (wantHelp) {
        printUsage(std::cout);
        return 0;
    }

    if (wantVersion) {
        std::cout << "reg " << kVersion << '\n';
        return 0;
    }

    if (verbose && quiet) {
        std::cerr << "reg: -v/--verbose and -q/--quiet are mutually exclusive\n";
        return 2;
    }

    int actions = 0;
    if (dumpPath.has_value())
        ++actions;
    if (!getPath.empty())
        ++actions;
    if (!setPath.empty())
        ++actions;
    if (!exportPath.empty())
        ++actions;
    if (!importPath.empty())
        ++actions;

    if (actions == 0) {
        printUsage(std::cerr);
        return 2;
    }
    if (actions > 1) {
        std::cerr << "reg: specify only one of --dump, --get, --set, --export, or --import\n";
        return 2;
    }

    if (verbose) {
        set_loglevel(LOG_LEVEL_DEBUG);
    } else if (quiet) {
        set_loglevel(LOG_LEVEL_ERROR);
    }

    RegistryContext ctx;
    if (!openRegistry(rootPath, ctx)) {
        return 2;
    }
    IRegistry& registry = *ctx.reg;

    int rc = 0;
    try {
        if (dumpPath.has_value()) {
            auto info = registry.stat(*dumpPath);
            if (!info.has_value()) {
                std::cerr << "reg: invalid path " << *dumpPath << '\n';
                rc = 1;
            } else {
                dumpTree(registry, *dumpPath, *info, std::cout, "");
            }
        } else if (!getPath.empty()) {
            auto opt = registry.getOption(getPath);
            if (!opt.has_value()) {
                std::cerr << "reg: no value at " << getPath << '\n';
                rc = 1;
            } else {
                std::cout << valueToString(*opt) << '\n';
            }
        } else if (!setPath.empty()) {
            auto parsed = parseOption(setValue);
            registry.setOption(setPath, parsed);
            if (!registry.has(setPath)) {
                std::cerr << "reg: failed to set " << setPath << '\n';
                rc = 1;
            } else {
                ctx.markDirty();
            }
        } else if (!exportPath.empty()) {
            const auto tree = exportNode(registry, exportPath);
            std::cout << boost::json::serialize(tree) << '\n';
        } else if (!importPath.empty()) {
            std::ostringstream input;
            input << std::cin.rdbuf();
            const auto jv = boost::json::parse(input.str());
            importJson(registry, importPath, jv);
            ctx.markDirty();
        }
    } catch (const std::exception& e) {
        std::cerr << "reg: " << e.what() << '\n';
        rc = 1;
    }

    try {
        ctx.flush();
    } catch (const std::exception& e) {
        std::cerr << "reg: " << e.what() << '\n';
        rc = 1;
    }

    return rc;
}

} // namespace bas::reg

int main(int argc, char** argv) { return bas::reg::main(argc, argv); }
