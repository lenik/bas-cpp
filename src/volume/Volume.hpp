#ifndef IVOLUME_H
#define IVOLUME_H

#include "FileStatus.hpp"
#include "VolumeFile.hpp"

#include "../io/InputStream.hpp"
#include "../io/OutputStream.hpp"
#include "../io/Reader.hpp"
#include "../io/Writer.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

enum class VolumeType {
    HARDDISK,
    FLOPPY,
    CDROM,
    NETWORK,
    ARCHIVE,
    SYSTEM,
    MEMORY,
    OTHER,
};

/**
 * Simplified generic volume interface for file system abstraction
 */
class Volume {
private:
    mutable std::string m_uuid;
    mutable std::string m_serial;
    mutable std::string m_label;
    mutable bool m_uuid_cached = false;
    mutable bool m_serial_cached = false;
    mutable bool m_label_cached = false;

protected:
    friend class AccessControlledVolume;
    virtual std::string getDefaultLabel() const = 0;

public:
    virtual ~Volume() = default;
    
    // Volume info
    virtual std::string getClass() const = 0;  // "local", "seczure", etc.
    virtual std::string getId() const = 0;
    inline std::string getClassId() const {
        return getClass() + ":" + getId();
    }
    virtual VolumeType getType() const = 0;
    virtual bool isLocal() const { return false; }
    virtual std::string getLocalFile(std::string_view path) const = 0;

    // default implementation: by file .rc/UUID
    virtual std::string getUUID();
    // default implementation: by file .rc/SERIAL
    virtual std::string getSerial();
    // default implementation: by file .rc/LABEL
    virtual std::string getLabel();
    virtual void setLabel(std::string_view label);
    
    std::unique_ptr<VolumeFile> getRootFile();
    std::unique_ptr<VolumeFile> resolve(std::string_view path);

    // normalized path: 
    //   always starts with /, no trailing slash, merge multiple slashes, remove . and .. tokens, etc.
    //   empty means not specified.
    // throws exception if path is empty and optional is false.
    virtual std::string normalize(std::string_view path, bool optional = false) const;

    // throws IOException if underlying error, path not found, etc.
    virtual std::string toRealPath(std::string_view path) const;
    
    // Basic file operations (testing methods - don't throw exceptions)
    virtual bool exists(std::string_view path) const = 0;
    virtual bool isFile(std::string_view path) const = 0;
    virtual bool isDirectory(std::string_view path) const = 0;
    virtual bool stat(std::string_view path, FileStatus* status) const = 0;
    
    // Directory operations
    // readDir can throw IOException or AccessException
    virtual void readDir(std::vector<std::unique_ptr<FileStatus>>& list, std::string_view path, bool recursive = false)
        { printf("readDir(list, path, rec) not implemented yet\n"); }

    std::vector<std::unique_ptr<FileStatus>> readDir(std::string_view path, bool recursive = false);
    //  {
    //     std::vector<std::unique_ptr<FileStatus>> list;
    //     readDir(list, path, recursive);
    //     return list;
    // }
    
    // return true if operation successful, false if directory exists.
    // throws IOException if underlying error, file with same name exists, permission denied,etc.
    virtual bool createDirectory(std::string_view path) = 0;
    virtual bool createDirectories(std::string_view path);
    virtual bool createParentDirectories(std::string_view path);

    // return true if actually removed, false if directory does not exist.
    // throws IOException if underlying error, path denotes a file, etc.
    virtual bool removeDirectory(std::string_view path) = 0;
    
    // return true if actually removed, false if file does not exist.
    // throws IOException if underlying error, path denotes a directory, permission denied, etc.
    bool removeFile(std::string_view path);

    // return true if actually copied, false if dest exists and overwrite is false.
    // throws IOException if underlying error, src does not exist, permission denied, etc.
    // if dest is a directory exists, the file is copied into the directory.
    bool copyFile(std::string_view src, std::string_view dest, bool overwrite = true);

    // return true if actually moved, false if dest is the same as src, or dest exists and overwrite is false.
    // throws IOException if underlying error, src does not exist or failed to remove, permission denied, etc.
    // if dest is a directory exists, the file is moved into the directory.
    bool moveFile(std::string_view src, std::string_view dest, bool overwrite = true);

    // return true if actually renamed, false if dest is the same as src, or dest exists and overwrite is false.
    // throws IOException if underlying error, rename across directories isn't supported, 
    //     src does not exist, permission denied, etc.
    // if dest is a directory exists, the file is moved into the directory. (if system supports it)
    bool rename(std::string_view src, std::string_view dest, bool overwrite = true);
    
    // File I/O
    // openForRead, openForWrite can throw IOException or AccessException
    // virtual std::unique_ptr<IReadStream> openForRead(std::string_view path, std::string_view encoding = "UTF-8") = 0;
    // virtual std::unique_ptr<IWriteStream> openForWrite(std::string_view path, bool append = false, std::string_view encoding = "UTF-8") = 0;
    virtual std::unique_ptr<InputStream> newInputStream(std::string_view path) = 0;
    virtual std::unique_ptr<OutputStream> newOutputStream(std::string_view path, bool append = false) = 0;
   
    // default: buffer & iconv
    virtual std::unique_ptr<Reader> newReader(std::string_view path, std::string_view encoding = "UTF-8");
    virtual std::unique_ptr<Writer> newWriter(std::string_view path, bool append = false, std::string_view encoding = "UTF-8");
    
    virtual std::unique_ptr<RandomInputStream> newRandomInputStream(std::string_view path) = 0;
    virtual std::unique_ptr<RandomReader> newRandomReader(std::string_view path, std::string_view encoding = "UTF-8");

    // readFile can throw IOException or AccessException
    virtual std::vector<uint8_t> readFile(std::string_view path) = 0;
    std::string readFileUTF8(std::string_view path);
    std::string readFileString(std::string_view path, std::string_view encoding = "UTF-8");
    std::deque<std::string> readLines(std::string_view path, int maxLines = -1, std::string_view encoding = "UTF-8");
    std::deque<std::string> readLastLines(std::string_view path, int maxLines = -1, std::string_view encoding = "UTF-8");

    // writeFile can throw IOException or AccessException
    virtual void writeFile(std::string_view path, const std::vector<uint8_t>& data) = 0;
    void writeFileUTF8(std::string_view path, std::string_view data);
    void writeFileString(std::string_view path, std::string_view data, std::string_view encoding = "UTF-8");
    void writeLines(std::string_view path, const std::vector<std::string>& lines, std::string_view encoding = "UTF-8");
    
    // Utility
    virtual std::string getTempDir() = 0;
    virtual std::string createTempFile(std::string_view prefix = "tmp.", std::string_view suffix = "") = 0;

protected:
    virtual void removeFileUnchecked(std::string_view path) = 0;
    virtual void copyFileUnchecked(std::string_view src, std::string_view dest) = 0;
    virtual void moveFileUnchecked(std::string_view src, std::string_view dest) = 0;
    virtual void renameFileUnchecked(std::string_view src, std::string_view dest) = 0;

private:
    std::string readRCFile(std::string_view name);
    bool writeRCFile(std::string_view name, std::string_view data);
};

inline std::string getHomePath() {
    std::string homePath = getenv("HOME");
    if (homePath.empty()) {
        #ifdef _WIN32
        homePath = getenv("USERPROFILE");
        #else
        std::string user = getenv("USER");
        if (user.empty()) {
            homePath = "/";
        } else {
            homePath = "/home/" + user;
        }
        #endif
    }
    return homePath;
}

#endif // IVOLUME_H
