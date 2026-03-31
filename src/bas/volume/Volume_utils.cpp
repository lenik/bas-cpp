#include "Volume.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <sys/stat.h>

std::string format_mode(int mode) {
    std::string s;
    s.reserve(10);

    // 1. File Type
    if (S_ISDIR(mode))
        s += 'd';
    else if (S_ISLNK(mode))
        s += 'l';
    else if (S_ISCHR(mode))
        s += 'c';
    else if (S_ISBLK(mode))
        s += 'b';
    else if (S_ISFIFO(mode))
        s += 'p';
    else if (S_ISSOCK(mode))
        s += 's';
    else
        s += '-';

    // 2. Owner Permissions
    s += (mode & S_IRUSR) ? 'r' : '-';
    s += (mode & S_IWUSR) ? 'w' : '-';
    s += (mode & S_IXUSR) ? 'x' : '-';

    // 3. Group Permissions
    s += (mode & S_IRGRP) ? 'r' : '-';
    s += (mode & S_IWGRP) ? 'w' : '-';
    s += (mode & S_IXGRP) ? 'x' : '-';

    // 4. Other Permissions
    s += (mode & S_IROTH) ? 'r' : '-';
    s += (mode & S_IWOTH) ? 'w' : '-';
    s += (mode & S_IXOTH) ? 'x' : '-';

    return s;
}

std::string format_size_human(size_t size) {
    static const std::vector<std::string> units = {"B", "K", "M", "G", "T", "P"};

    if (size < 1024) {
        return std::to_string(size) + units[0];
    }

    int unit_index = 0;
    double d_size = static_cast<double>(size);

    while (d_size >= 1024 && unit_index < static_cast<int>(units.size()) - 1) {
        d_size /= 1024.0;
        unit_index++;
    }

    std::stringstream ss;
    // Set precision to 1 decimal place (e.g., 1.5G)
    ss << std::fixed << std::setprecision(1) << d_size << units[unit_index];
    return ss.str();
}

std::string format_size(size_t size, bool human) {
    if (human) {
        return format_size_human(size);
    }

    std::stringstream ss;
    // This injects commas (e.g., 1,234,567) using the user's locale
    ss.imbue(std::locale(""));
    ss << std::fixed << size;
    return ss.str();
}

std::string format_time(int64_t epochNano) {
    auto time = std::chrono::system_clock::time_point(std::chrono::nanoseconds(epochNano));
    auto zoned = date::zoned_time{date::current_zone(), time};
    auto local = zoned.get_local_time();
    return date::format("%Y-%m-%d %H:%M:%S", local);
}

const ListOptions ListOptions::DEFAULT;

ListOptions ListOptions::parse(std::string_view options) {
    ListOptions opts;
    for (char c : options) {
        switch (c) {
        case 'R':
            opts.recursive = true;
            break;
        case 'l':
            opts.longFormat = true;
            break;
        case 'a':
            opts.includeDotFiles = opts.includeDotDot = true;
            break;
        case 'A':
            opts.includeDotFiles = true;
            break;
        case 'F':
            opts.formatSuffix = true;
            break;
        case 'h':
            opts.human = true;
            break;
        case 'C':
            opts.color = true;
            break;
        case 'T':
            opts.tree = true;
            break;
        }
    }
    return opts;
}

static std::unordered_map<unsigned int, std::string> uid_map;
static std::unordered_map<unsigned int, std::string> gid_map;
static bool loaded_from_passwd;
static bool loaded_from_group;

std::string format_uid(unsigned int uid) {
    std::string passwd_file = "/etc/passwd";
    if (!loaded_from_passwd) {
        loaded_from_passwd = true;
        if (!std::filesystem::exists(passwd_file)) {
            std::ifstream passwd_stream(passwd_file);
            std::string line;
            while (std::getline(passwd_stream, line)) {
                std::istringstream iss(line);
                std::string username, password, uid, gid, info, home, shell;
                std::getline(iss, username, ':');
                std::getline(iss, password, ':');
                std::getline(iss, uid, ':');
                std::getline(iss, gid, ':');
                std::getline(iss, info, ':');
                std::getline(iss, home, ':');
                std::getline(iss, shell, ':');
                uid_map[std::stoi(uid)] = username;
            }
        }
    }
    if (uid_map.find(uid) != uid_map.end()) {
        return uid_map[uid];
    }
    switch (uid) {
    case 0:
        return "root";
    case 1:
        return "daemon";
    case 2:
        return "bin";
    case 3:
        return "sync";
    case 4:
        return "sync";
    case 5:
        return "games";
    case 6:
        return "man";
    case 7:
        return "lp";
    case 8:
        return "mail";
    case 9:
        return "news";
    case 10:
        return "uucp";
    case 13:
        return "proxy";
    default:
        return std::to_string(uid);
    }
    return std::to_string(uid);
}

std::string format_gid(unsigned int gid) {
    std::string group_file = "/etc/group";
    if (!loaded_from_group) {
        loaded_from_group = true;
        if (!std::filesystem::exists(group_file)) {
            std::ifstream group_stream(group_file);
            std::string line;
            while (std::getline(group_stream, line)) {
                std::istringstream iss(line);
                std::string groupname, password, gid, members;
                std::getline(iss, groupname, ':');
                std::getline(iss, password, ':');
                std::getline(iss, gid, ':');
                std::getline(iss, members, ':');
                gid_map[std::stoi(gid)] = groupname;
            }
        }
    }
    if (gid_map.find(gid) != gid_map.end()) {
        return gid_map[gid];
    }
    switch (gid) {
    case 0:
        return "root";
    case 1:
        return "daemon";
    case 2:
        return "bin";
    case 3:
        return "sync";
    case 4:
        return "sync";
    case 5:
        return "games";
    case 6:
        return "man";
    case 7:
        return "lp";
    case 8:
        return "mail";
    case 9:
        return "news";
    case 10:
        return "uucp";
    case 13:
        return "proxy";
    default:
        return std::to_string(gid);
    }
    return std::to_string(gid);
}

void Volume::ls(std::string_view path, std::optional<ListOptions> options) {
    ListOptions opts = options.value_or(ListOptions::DEFAULT);
    if (opts.tree) {
        this->tree(path, "", opts);
        return;
    }

    bool accessTime = false;
    bool createTime = false;

    std::vector<std::unique_ptr<DirNode>> list = readDir(path, opts.recursive);

    int max_size_width = 0;
    int max_user_width = 0;
    int max_group_width = 0;
    int max_time_width = 0;
    int max_name_width = 0;
    for (const auto& st : list) {
        int size_width = format_size(st->size, opts.human).size();
        int user_width = format_uid(st->uid).size();
        int group_width = format_gid(st->gid).size();
        int time_width = format_time(st->modifiedNano()).size();
        int name_width = st->name.size();

        if (name_width > max_name_width)
            max_name_width = name_width;
        if (size_width > max_size_width)
            max_size_width = size_width;
        if (user_width > max_user_width)
            max_user_width = user_width;
        if (group_width > max_group_width)
            max_group_width = group_width;
        if (time_width > max_time_width)
            max_time_width = time_width;
    }

    const char* env_columns = getenv("COLUMNS");
    int columns = env_columns ? std::stoi(env_columns) : 80;
    int names_per_line = columns / (max_name_width + 1);

    int count_in_line = 0;
    for (const auto& st : list) {
        if (st->name.front() == '.') {
            if (!opts.includeDotFiles)
                continue;
            if (st->name == "." || st->name == "..")
                if (!opts.includeDotDot)
                    continue;
        }

        std::string suffix = "";
        std::string color_name = "";
        std::string color_mode = "";
        std::string color_size = "";
        std::string color_user = "";
        std::string color_group = "";
        std::string color_time = "";
        std::string color_reset = opts.color ? opts.color_end : "";
        if (opts.color) {
            color_mode = opts.color_mode;
            color_size = opts.color_size;
            color_user = opts.color_user;
            color_group = opts.color_group;
            color_time = opts.color_time;
        }

        if (opts.formatSuffix || opts.color) {
            color_name = opts.color_regular;
            if (st->isDirectory()) {
                suffix = "/";
                color_name = opts.color_dir;
            } else if (st->isSymbolicLink()) {
                suffix = "->";
                color_name = opts.color_link;
            } else if (st->isFIFO()) {
                suffix = "|";
                color_name = opts.color_fifo;
            } else if (st->isSocket()) {
                suffix = "=";
                color_name = opts.color_socket;
            } else if (st->isCharacterDevice()) {
                suffix = "c";
                color_name = opts.color_character;
            } else if (st->isBlockDevice()) {
                suffix = "b";
                color_name = opts.color_block;
            } else if (st->isRegularFile()) {
                if (st->mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
                    suffix = "*";
                    color_name = opts.color_regular;
                }
            }
            if (!opts.formatSuffix)
                suffix = "";
            if (!opts.color)
                color_name = "";
        }

        std::string childPath(path);
        if (!childPath.empty())
            if (childPath.back() != '/')
                childPath += "/";
        childPath += st->name;

        if (opts.longFormat) {
            std::string mode = format_mode(st->mode);
            std::cout << color_mode << std::setw(10) << mode << color_reset;

            std::string size = format_size(st->size, opts.human);
            std::cout << " " << color_size << std::setw(max_size_width + 1) << size << color_reset;

            std::string user = format_uid(st->uid);
            std::cout << " " << color_user << std::setw(max_user_width + 1) << user << color_reset;

            std::string group = format_gid(st->gid);
            std::cout << color_group << std::setw(max_group_width + 1) << group << color_reset;

            std::string modifiedTime = format_time(st->modifiedNano());
            if (accessTime)
                (void)format_time(st->accessNano());
            if (createTime)
                (void)format_time(st->creationNano());
            std::cout << " " << color_time << std::setw(max_time_width + 1) << modifiedTime
                      << color_reset;

            std::cout << " " << color_name << childPath << color_reset;
            if (opts.formatSuffix) {
                std::cout << opts.color_suffix << suffix << color_reset;
            }
            std::cout << std::endl;
        } else {
            std::cout << std::setw(max_name_width + 1) << color_name << childPath << color_reset;
            if (opts.formatSuffix)
                std::cout << opts.color_suffix << suffix << color_reset;
            count_in_line++;
            if (count_in_line >= names_per_line) {
                std::cout << std::endl;
                count_in_line = 0;
            }
        }
    } // for
    if (count_in_line > 0) {
        std::cout << std::endl;
    }
    // if (opts.recursive) {
    //     for (const auto& st : list) {
    //         if (st->name.front() == '.') {
    //             if (st->name == "." || st->name == "..")
    //                 continue;
    //             if (!opts.includeDotFiles)
    //                 continue;
    //         }
    //         if (st->isDirectory()) {
    //             std::string childPath(path);
    //             if (!childPath.empty())
    //                 if (childPath.back() != '/')
    //                     childPath += "/";
    //             childPath += st->name;
    //             ls(childPath, opts);
    //         }
    //     }
    // }
}

void Volume::tree(std::string_view path, const std::string& prefix,
                  std::optional<ListOptions> options) {
    std::vector<std::unique_ptr<DirNode>> list;
    try {
        list = this->readDir(path);
    } catch (const std::exception& e) {
        std::cerr << "Error reading directory " << path << ": " << e.what() << std::endl;
        return;
    }

    ListOptions opts = options.value_or(ListOptions::DEFAULT);
    std::string color_reset = opts.color ? opts.color_end : "";
    std::string color_name = opts.color ? opts.color_regular : "";
    std::string color_suffix = opts.color ? opts.color_suffix : "";

    for (const auto& st : list) {
        if (!st)
            continue;
        bool isDir = st->isDirectory();
        std::string line = prefix;
        line += st->name;
        if (isDir)
            line += "/";
        if (st->isRegularFile() && st->size) {
            char buf[32];
            snprintf(buf, sizeof(buf), " (%zu)", static_cast<size_t>(st->size));
            line += buf;
        }
        std::puts(line.c_str());
        if (isDir) {
            std::string childPath(path);
            if (!childPath.empty())
                if (childPath.back() != '/')
                    childPath += "/";
            childPath += st->name;
            tree(childPath, prefix + "  ");
        }
    }
}
