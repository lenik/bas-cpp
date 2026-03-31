#ifndef FILESTATUS_H
#define FILESTATUS_H

#include "DirEntry.hpp"

#include <cstdint>
#include <ctime>

class FileStatus : public DirEntry {
public:
    FileStatus() 
        : accessTime(0)
        , mode(0)
        , dev(0)
        , ino(0)
        , nlink(0)
        , rdev(0)
        , blksize(0)
        , blocks(0)
    {}
    
    FileStatus(const DirEntry& entry)
        : DirEntry(entry)
        , accessTime(entry.modifiedTime)  // Use modifiedTime as fallback
        , mode(0)
        , dev(0)
        , ino(0)
        , nlink(0)
        , rdev(0)
        , blksize(0)
        , blocks(0)
    {}
    
    time_t accessTime;
    
    int mode;
    uint64_t dev;
    uint64_t ino;
    unsigned int nlink;
    uint64_t rdev;
    uint64_t blksize;
    uint64_t blocks;
};

#endif // VSTATS_H

