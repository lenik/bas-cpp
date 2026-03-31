#ifndef DIRENTRY_H
#define DIRENTRY_H

#include <cstdint>
#include <ctime>
#include <string>

enum FileType {
    REGULAR_FILE = 1,
    DIRECTORY = 2,
    SYMBOLIC_LINK = 4,
    BLOCK_DEVICE = 8,
    CHARACTER_DEVICE = 16,
    FIFO = 32,
    SOCKET = 64,
};

class DirEntry {
public:
    DirEntry()
        : name()
        , type(REGULAR_FILE)
    {}
    
    DirEntry(std::string_view name, FileType type = REGULAR_FILE)
        : name(name)
        , type(type)
    {}
    
    std::string name;
    int type;
    uint64_t size{0};
    time_t modifiedTime{0};
    time_t creationTime{0};
    unsigned int uid{0};
    unsigned int gid{0};

public:
    inline bool isRegularFile() const { return (type & REGULAR_FILE) != 0; }
    inline bool isDirectory() const { return (type & DIRECTORY) != 0; }
    inline bool isSymbolicLink() const { return (type & SYMBOLIC_LINK) != 0; }
    inline bool isBlockDevice() const { return (type & BLOCK_DEVICE) != 0; }
    inline bool isCharacterDevice() const { return (type & CHARACTER_DEVICE) != 0; }
    inline bool isFIFO() const { return (type & FIFO) != 0; }
    inline bool isSocket() const { return (type & SOCKET) != 0; }
};

#endif // DIRENTRY_H
