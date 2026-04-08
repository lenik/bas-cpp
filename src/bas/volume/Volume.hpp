#ifndef IVOLUME_H
#define IVOLUME_H

#include "DirNode.hpp"
#include "VolumeFile.hpp"

#include "../io/InputStream.hpp"
#include "../io/OutputStream.hpp"
#include "../io/Reader.hpp"
#include "../io/Writer.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unconst>
#include <vector>

enum class VolumeType {
    HARDDISK,
    // PORTABLE,
    FLOPPY,
    CDROM,
    NETWORK,
    ARCHIVE,
    SYSTEM,
    MEMORY,
    OTHER,
};

std::string volumeTypeToString(VolumeType t);

struct ListOptions {
    bool recursive = false;
    bool longFormat = false;
    bool includeDotFiles = false;
    bool includeDotDot = false;
    bool formatSuffix = false;
    bool human = false;
    bool color = false;
    bool tree = false;

    std::string color_dir = "\033[34m";
    std::string color_link = "\033[32m";
    std::string color_fifo = "\033[33m";
    std::string color_socket = "\033[35m";
    std::string color_character = "\033[36m";
    std::string color_block = "\033[37m";
    std::string color_regular = "\033[39m";
    std::string color_regular_executable = "\033[32m";
    std::string color_regular_archive = "\033[33m";
    std::string color_suffix = "\033[0m";
    std::string color_end = "\033[0m";

    std::string color_mode = "\033[34m";
    std::string color_size = "\033[35m";
    std::string color_user = "\033[32m";
    std::string color_group = "\033[33m";
    std::string color_time = "\033[36m";

    static ListOptions parse(std::string_view options);
    static const ListOptions DEFAULT;
};

/**
 * Simplified generic volume interface for file system abstraction
 */
class Volume {
  protected:
    friend class AccessControlledVolume;
    virtual std::string getDefaultLabel() const = 0;

  public:
    virtual ~Volume() = default;

    int getPriority() const { return m_priority; }
    void setPriority(int priority) { m_priority = priority; }

    // Volume info
    virtual std::string getClass() const = 0; // "local", "seczure", etc.
    virtual std::string getUrl() const = 0;
    virtual std::string getDeviceUrl() const = 0;

    virtual VolumeType getType() const = 0;
    virtual std::string getTypeString() const;

    virtual bool isEncrypted() const { return false; }
    virtual bool isLocal() const { return false; }
    virtual std::optional<std::string> getLocalFile(std::string_view path) const { return std::nullopt; }

    // default implementation: by file .rc/UUID
    virtual std::string getUUID() const;

    // default implementation: by file .rc/SERIAL
    virtual std::string getSerial() const;

    // default implementation: by file .rc/LABEL
    virtual std::string getLabel() const;
    virtual void setLabel(std::string_view label);

    std::unique_ptr<const VolumeFile> getRootFile() const;
    std::unique_ptr<VolumeFile> getRootFile();
    
    std::unique_ptr<const VolumeFile> resolve(std::string_view path) const;
    std::unique_ptr<VolumeFile> resolve(std::string_view path);

    // check if path is valid, returns normalized form.
    // empty means not specified, returns empty.
    // throws exception if path is empty and optional is false.
    std::string normalizeArg(std::string_view path,
                             std::optional<std::string> fallback = std::nullopt) const;

    // normalized path:
    // @path not empty.
    // @return  always starts with /, no trailing slash, merge multiple slashes, remove . and ..
    // tokens, etc.
    std::string normalize(std::string_view path) const;

    // throws IOException if underlying error, path not found, etc.
    virtual std::string toRealPath(std::string_view path) const;

    // Basic file operations (testing methods - don't throw exceptions)
    virtual bool exists(std::string_view path) const = 0;
    virtual bool isFile(std::string_view path) const = 0;
    virtual bool isDirectory(std::string_view path) const = 0;
    virtual bool stat(std::string_view path, DirNode* status) const = 0;

    // Directory operations
    // readDir can throw IOException or AccessException
    virtual void readDir_inplace(DirNode& context, std::string_view path, bool recursive = false) {
        printf("readDir_inplace(context, path, recursive) not implemented yet\n");
    }

    virtual std::unique_ptr<DirNode> readDir(std::string_view path, bool recursive = false);

    // return true if operation successful, false if directory exists.
    // throws IOException if underlying error, file with same name exists, permission denied,etc.
    bool createDirectory(std::string_view path);
    void createDirectoryThrows(std::string_view path);

    bool createDirectories(std::string_view path);
    void createDirectoriesThrows(std::string_view path);
    bool createParentDirectories(std::string_view path);

    // return true if actually removed, false if directory does not exist.
    // throws IOException if underlying error, path denotes a file, etc.
    bool removeDirectory(std::string_view path);
    void removeDirectoryThrows(std::string_view path);

    // return true if actually removed, false if file does not exist.
    // throws IOException if underlying error, path denotes a directory, permission denied, etc.
    bool removeFile(std::string_view path);
    void removeFileThrows(std::string_view path);

    // return true if actually copied, false if dest exists and overwrite is false.
    // throws IOException if underlying error, src does not exist, permission denied, etc.
    // if dest is a directory exists, the file is copied into the directory.
    bool copyFile(std::string_view src, std::string_view dest, bool overwrite = true);
    void copyFileThrows(std::string_view src, std::string_view dest, bool overwrite = true);

    // return true if actually moved, false if dest is the same as src, or dest exists and overwrite
    // is false. throws IOException if underlying error, src does not exist or failed to remove,
    // permission denied, etc. if dest is a directory exists, the file is moved into the directory.
    bool moveFile(std::string_view src, std::string_view dest, bool overwrite = true);
    void moveFileThrows(std::string_view src, std::string_view dest, bool overwrite = true);

    // return true if actually renamed, false if dest is the same as src, or dest exists and
    // overwrite is false. throws IOException if underlying error, rename across directories isn't
    // supported,
    //     src does not exist, permission denied, etc.
    // if dest is a directory exists, the file is moved into the directory. (if system supports it)
    bool rename(std::string_view src, std::string_view dest, bool overwrite = true);
    void renameFileThrows(std::string_view src, std::string_view dest, bool overwrite = true);

    // File I/O
    // openForRead, openForWrite can throw IOException or AccessException
    // virtual std::unique_ptr<IReadStream> openForRead(std::string_view path, std::string_view
    // encoding = "UTF-8") = 0; virtual std::unique_ptr<IWriteStream> openForWrite(std::string_view
    // path, bool append = false, std::string_view encoding = "UTF-8") = 0;
    virtual std::unique_ptr<InputStream> newInputStream(std::string_view path) = 0;
    virtual std::unique_ptr<OutputStream> newOutputStream(std::string_view path,
                                                          bool append = false) = 0;

    // default: buffer & iconv
    virtual std::unique_ptr<Reader> newReader(std::string_view path,
                                              std::string_view encoding = "UTF-8");
    virtual std::unique_ptr<Writer> newWriter(std::string_view path, bool append = false,
                                              std::string_view encoding = "UTF-8");

    virtual std::unique_ptr<RandomInputStream> newRandomInputStream(std::string_view path) = 0;
    virtual std::unique_ptr<RandomReader> newRandomReader(std::string_view path,
                                                          std::string_view encoding = "UTF-8");

    /**
     * @param len maximum size in bytes of the data to read (0 means read all)
     * @param off byte offset from start when >= 0; when negative, from EOF+1
     *               (e.g. -1 = EOF -> read nothing, -11 with len=0 -> last 10 bytes)
     * @param default_data if file does not exist
     * @return data read from the file
     */
    std::vector<uint8_t> readFile(std::string_view path, int64_t off = 0, size_t len = 0,
                                  std::optional<std::vector<uint8_t>> default_data = std::nullopt);

    std::string readFileUTF8(std::string_view path,
                             std::optional<std::string> default_data = std::nullopt);

    std::string readFileString(std::string_view path, std::string_view encoding = "UTF-8",
                               std::optional<std::string> default_data = std::nullopt);

    std::deque<std::string> readLines(std::string_view path, int maxLines = -1,
                                      std::string_view encoding = "UTF-8");
    std::deque<std::string> readLastLines(std::string_view path, int maxLines = -1,
                                          std::string_view encoding = "UTF-8");

    // writeFile can throw IOException or AccessException
    void writeFile(std::string_view path, const std::vector<uint8_t>& data);
    void writeFileUTF8(std::string_view path, std::string_view data);
    void writeFileString(std::string_view path, std::string_view data,
                         std::string_view encoding = "UTF-8");
    void writeLines(std::string_view path, const std::vector<std::string>& lines,
                    std::string_view encoding = "UTF-8");

    // Utility
    virtual std::string getTempDir() = 0;
    virtual std::string createTempFile(std::string_view prefix = "tmp.",
                                       std::string_view suffix = "") = 0;

    void ls(std::string_view path, std::optional<ListOptions> options = std::nullopt);
    void tree(std::string_view path, const std::string& prefix = "",
              std::optional<ListOptions> options = std::nullopt);

  protected:
    friend class OverlayVolume;
    friend class AccessControlledVolume;

    virtual void setUUIDForced(std::string_view u);
    virtual void setSerialForced(std::string_view s);

    // unchecked operations: no exception handling, no return value

    /**
     * @param path path to the file
     * @param off byte offset from start when >= 0; when negative, from EOF+1
     *               (e.g. -1 = EOF -> read nothing, -11 with len=0 -> last 10 bytes)
     * @param len maximum size in bytes of the data to read (0 means read all)
     * @return data read from the file
     */
    virtual std::vector<uint8_t> readFileUnchecked(std::string_view path, int64_t off = 0,
                                                   size_t len = 0) = 0;
    virtual void writeFileUnchecked(std::string_view path, const std::vector<uint8_t>& data) = 0;

    virtual void createDirectoryThrowsUnchecked(std::string_view path) = 0;
    virtual void removeDirectoryThrowsUnchecked(std::string_view path) = 0;
    virtual void removeFileThrowsUnchecked(std::string_view path) = 0;
    virtual void copyFileThrowsUnchecked(std::string_view src, std::string_view dest) = 0;
    virtual void moveFileThrowsUnchecked(std::string_view src, std::string_view dest) = 0;
    virtual void renameFileThrowsUnchecked(std::string_view src, std::string_view dest) = 0;

  private:
    std::string readRCFile(std::string_view name) const;
    bool writeRCFile(std::string_view name, std::string_view data);

  private:
    __unconst std::string c_uuid;
    __unconst std::string c_serial;
    __unconst std::string c_label;
    __unconst bool c_uuid_valid = false;
    __unconst bool c_serial_valid = false;
    __unconst bool c_label_valid = false;

    int m_priority = 0;
    std::string m_uuidFile = "UUID";
    std::string m_serialFile = "SERIAL";
    std::string m_labelFile = "LABEL";
    
    // FSLang integration
    struct ExecutionResult {
        bool success = false;
        std::string error;
        int failedLine = 0;
    };
    
    ExecutionResult run(const std::string& fslangSource, 
                       std::optional<std::function<void()>> commitCallback = std::nullopt);
    
    std::string deepHash() const;
};

#endif // IVOLUME_H
