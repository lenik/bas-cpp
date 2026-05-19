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
#include <vector>

namespace {

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

std::string joinPath(std::string_view base, std::string_view name) {
    if (base.empty())
        return std::string(name);
    std::string out(base);
    if (out.back() != '/')
        out.push_back('/');
    out.append(name);
    return out;
}

void dumpTree(const bas::reg::IRegistry& reg, std::string_view path, std::ostream& out) {
    const auto children = reg.list(path);
    if (children.empty()) {
        auto opt = reg.getOption(path);
        if (!opt.has_value())
            return;
        out << path << '=' << bas::reg::optionToString(opt) << '\n';
        return;
    }
    for (const auto& child : children) {
        dumpTree(reg, joinPath(path, child), out);
    }
}

boost::json::value exportNode(const bas::reg::IRegistry& reg, std::string_view path) {
    const auto children = reg.list(path);
    if (children.empty()) {
        auto opt = reg.getOption(path);
        if (!opt.has_value())
            return boost::json::value(nullptr);
        return bas::reg::optionToJson(opt);
    }

    boost::json::object obj;
    for (const auto& child : children) {
        const std::string childPath = joinPath(path, child);
        obj.emplace(child, exportNode(reg, childPath));
    }
    return obj;
}

void importJson(bas::reg::IRegistry& reg, std::string_view path, const boost::json::value& jv) {
    if (jv.is_object()) {
        for (const auto& [key, value] : jv.as_object()) {
            importJson(reg, joinPath(path, std::string(key)), value);
        }
        return;
    }
    if (jv.is_array()) {
        const auto& arr = jv.as_array();
        for (size_t i = 0; i < arr.size(); ++i) {
            importJson(reg, joinPath(path, std::to_string(i)), arr[i]);
        }
        return;
    }
    reg.setOption(path, bas::reg::jsonToOption(jv));
}

void loadJsonRegistryFile(bas::reg::JsonRegistry& reg, const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open " + path.string());
    }
    std::ostringstream content;
    content << in.rdbuf();
    const std::string text = content.str();
    if (text.empty()) {
        boost::json::object empty;
        reg.jsonIn(empty, JsonFormOptions::DEFAULT);
        return;
    }
    const auto jv = boost::json::parse(text);
    if (!jv.is_object()) {
        throw std::runtime_error("JSON registry file root must be an object: " + path.string());
    }
    auto obj = jv.as_object();
    reg.jsonIn(obj, JsonFormOptions::DEFAULT);
}

void saveJsonRegistryFile(bas::reg::JsonRegistry& reg, const std::filesystem::path& path) {
    boost::json::object obj;
    reg.jsonOut(obj, JsonFormOptions::DEFAULT);
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to write " + path.string());
    }
    out << boost::json::serialize(obj);
}

struct RegistryContext {
    std::unique_ptr<LocalVolume> localVolume;
    std::unique_ptr<bas::reg::VolumeRegistry> volume;
    std::optional<bas::reg::JsonRegistry> json;
    bas::reg::IRegistry* reg = nullptr;
    bas::reg::IContainerManager* containers = nullptr;
    std::filesystem::path jsonFile;
    bool jsonDirty = false;

    void markDirty() { jsonDirty = true; }

    void flush() {
        if (containers) {
            containers->flushCachedContainers();
        } else if (!jsonFile.empty() && jsonDirty && json) {
            saveJsonRegistryFile(*json, jsonFile);
        }
    }
};

bool openRegistry(const std::string& rootOpt, RegistryContext& ctx) {
    if (rootOpt.empty()) {
        ctx.reg = &bas::reg::LocalRegistry::userDefault();
        ctx.containers = dynamic_cast<bas::reg::IContainerManager*>(ctx.reg);
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
        ctx.localVolume = std::make_unique<LocalVolume>(rootPath.string());
        ctx.volume = std::make_unique<bas::reg::VolumeRegistry>(
            VolumeFile(ctx.localVolume.get(), std::string()));
        ctx.reg = ctx.volume.get();
        ctx.containers = dynamic_cast<bas::reg::IContainerManager*>(ctx.reg);
        return true;
    }
    if (fs::is_regular_file(rootPath, ec)) {
        ctx.json.emplace();
        try {
            loadJsonRegistryFile(*ctx.json, rootPath);
        } catch (const std::exception& e) {
            std::cerr << "reg: " << e.what() << '\n';
            return false;
        }
        ctx.reg = &*ctx.json;
        ctx.jsonFile = rootPath;
        return true;
    }

    std::cerr << "reg: root is neither a directory nor a regular file: " << rootOpt << '\n';
    return false;
}

} // namespace

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
    bas::reg::IRegistry& registry = *ctx.reg;

    int rc = 0;
    try {
        if (dumpPath.has_value()) {
            dumpTree(registry, *dumpPath, std::cout);
        } else if (!getPath.empty()) {
            auto opt = registry.getOption(getPath);
            if (!opt.has_value()) {
                std::cerr << "reg: no value at " << getPath << '\n';
                rc = 1;
            } else {
                std::cout << bas::reg::valueToString(*opt) << '\n';
            }
        } else if (!setPath.empty()) {
            auto parsed = bas::reg::parseOption(setValue);
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
