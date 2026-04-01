#ifndef DIRENTRY_H
#define DIRENTRY_H

#include <date/tz.h>

#include <cstdint>
#include <ctime>
#include <string>

enum class FileType : uint8_t {
    Unknown = 0,
    Regular,
    Directory,
    Symlink,
    CharacterDevice,
    BlockDevice,
    FIFO,
    Socket
};

struct DirEntry {

    int64_t ino{0};
    std::string name;
    FileType type{FileType::Unknown};
    int64_t size{0};

    // Nanoseconds since Unix Epoch (1970-01-01)
    int64_t epochNano{0};

  public:
    DirEntry() = default;

    DirEntry(std::string_view name, FileType type = FileType::Unknown, int64_t size = 0,
             int64_t epochNano = 0)
        : name(name), type(type), size(size), epochNano(epochNano) {}

  public:
    inline bool isRegularFile() const { return type == FileType::Regular; }
    inline bool isDirectory() const { return type == FileType::Directory; }
    inline bool isSymbolicLink() const { return type == FileType::Symlink; }
    inline bool isBlockDevice() const { return type == FileType::BlockDevice; }
    inline bool isCharacterDevice() const { return type == FileType::CharacterDevice; }
    inline bool isFIFO() const { return type == FileType::FIFO; }
    inline bool isSocket() const { return type == FileType::Socket; }

    inline void clear() {
        type = FileType::Unknown;
        name.clear();
        size = 0;
        ino = 0;
        epochNano = 0;
    }
    
    inline void setUnknown() {
        type = FileType::Unknown;
    }
    inline void setRegular() {
        type = FileType::Regular;
    }
    inline void setDirectory() {
        type = FileType::Directory;
    }
    
    inline int64_t epochSeconds() const { return epochNano / 1'000'000'000LL; }
    inline void epochSeconds(int64_t seconds) { epochNano = seconds * 1'000'000'000LL; }

    inline std::chrono::system_clock::time_point epochTime() const {
        auto duration = std::chrono::nanoseconds(epochNano);
        return std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(duration));
    }

    inline void epochTime(std::chrono::system_clock::duration duration) {
        epochNano = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }
    inline void epochTime(std::chrono::system_clock::time_point time) {
        epochTime(time.time_since_epoch());
    }

    inline auto zonedTime() const { return date::zoned_time{date::current_zone(), epochTime()}; }
    inline auto localTime() const { return zonedTime().get_local_time(); }

    inline void zonedTime(date::zoned_time<std::chrono::system_clock::duration> zoned) {
        epochTime(zoned.get_sys_time());
    }
    inline void localTime(date::local_time<std::chrono::system_clock::duration> local) {
        auto zoned = date::zoned_time{date::current_zone(), local};
        epochTime(zoned.get_sys_time());
    }
};

#endif // DIRENTRY_H
