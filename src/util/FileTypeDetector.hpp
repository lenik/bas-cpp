#ifndef FILETYPEDETECTOR_H
#define FILETYPEDETECTOR_H

#include <cstdint>
#include <string>
#include <vector>

/**
 * File type detection utility functions
 */
class FileTypeDetector {
public:
    enum FileType {
        UNKNOWN,
        TEXT,
        IMAGE,
        AUDIO,
        VIDEO,
        DOCUMENT,
        SPREADSHEET,
        DATABASE,
        FS_IMAGE,
        ARCHIVE,
        EXECUTABLE,
        DIRECTORY
    };
    
    // File type detection
    static FileType detectFileType(std::string_view filename);
    static FileType detectFileTypeByContent(const std::vector<uint8_t>& data);
    static FileType detectFileTypeByExtension(std::string_view extension);
    
    // MIME type detection
    static std::string getMimeType(std::string_view filename);
    static std::string getMimeTypeByContent(const std::vector<uint8_t>& data);
    static std::string getMimeTypeByExtension(std::string_view extension);
    
    // File category checks
    static bool isTextFile(std::string_view filename);
    static bool isImageFile(std::string_view filename);
    static bool isAudioFile(std::string_view filename);
    static bool isVideoFile(std::string_view filename);
    static bool isDocumentFile(std::string_view filename);
    static bool isSpreadsheetFile(std::string_view filename);
    static bool isDatabaseFile(std::string_view filename);
    static bool isFsImageFile(std::string_view filename);
    static bool isArchiveFile(std::string_view filename);
    static bool isExecutableFile(std::string_view filename);
    
    // Content analysis
    static bool isTextContent(const std::vector<uint8_t>& data);
    static bool isBinaryContent(const std::vector<uint8_t>& data);
    static std::string getFileDescription(std::string_view filename);
    
    // Icon/representation
    static std::string getFileTypeString(FileType type);
    
    std::string getIcon(std::string_view filename, bool color = false);
    std::string getMonoIcon(std::string_view filename);
    std::string getColorIcon(std::string_view filename);
    std::string getFolderIcon(bool color = false);
    std::string getFolderMonoIcon();
    std::string getFolderColorIcon();
    
private:
    static std::string getFileExtension(std::string_view filename);
    static bool hasSignature(const std::vector<uint8_t>& data, const std::vector<uint8_t>& signature);

    std::string m_monoStyle = "/asset/heroicons/normal/";
    std::string m_colorStyle = "/asset/streamline-vectors/core/pop/";
};

#endif // FILETYPEDETECTOR_H
