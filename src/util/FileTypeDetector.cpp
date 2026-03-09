#include "FileTypeDetector.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

std::string FileTypeDetector::getFileExtension(std::string_view filename) {
    size_t dotPos = filename.find_last_of('.');
    if (dotPos == std::string::npos || dotPos == filename.length() - 1) {
        return "";
    }
    std::string ext(filename.substr(dotPos + 1));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

FileTypeDetector::FileType FileTypeDetector::detectFileTypeByExtension(std::string_view extension) {
    std::string ext(extension);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Directories
    if (ext.empty() || ext == "dir") return DIRECTORY;
    
    // Images
    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" || 
        ext == "svg" || ext == "webp" || ext == "ico" || ext == "bmp" || 
        ext == "tiff" || ext == "tif") return IMAGE;
    
    // Audio
    if (ext == "mp3" || ext == "wav" || ext == "ogg" || ext == "flac" || 
        ext == "aac" || ext == "m4a" || ext == "wma") return AUDIO;
    
    // Video
    if (ext == "mp4" || ext == "avi" || ext == "mov" || ext == "webm" || 
        ext == "mkv" || ext == "flv" || ext == "wmv" || ext == "m4v") return VIDEO;
    
    // Spreadsheets (must come before DOCUMENT)
    if (ext == "xls" || ext == "xlsx" || ext == "xlsm" || ext == "xlsb" || 
        ext == "ods" || ext == "csv" || ext == "tsv" || ext == "numbers") return SPREADSHEET;
    
    // Database files
    if (ext == "db" || ext == "sqlite" || ext == "sqlite3" || ext == "db3" || 
        ext == "sql" || ext == "mdb" || ext == "accdb" || ext == "dbf" || 
        ext == "odb" || ext == "fdb" || ext == "gdb") return DATABASE;
    
    // Filesystem images
    if (ext == "iso" || ext == "img" || ext == "dmg" || ext == "vdi" || 
        ext == "vmdk" || ext == "vhd" || ext == "vhdx" || ext == "qcow2" || 
        ext == "raw" || ext == "bin" || ext == "nrg" || ext == "mdf" || 
        ext == "cdr" || ext == "dmg" || ext == "toast" || ext == "cso") return FS_IMAGE;
    
    // Documents
    if (ext == "pdf" || ext == "doc" || ext == "docx" || 
        ext == "ppt" || ext == "pptx" || ext == "odt" || ext == "odp") return DOCUMENT;
    
    // Archives
    if (ext == "zip" || ext == "tar" || ext == "gz" || ext == "bz2" || 
        ext == "xz" || ext == "7z" || ext == "rar" || ext == "z" || 
        ext == "lz" || ext == "lzma") return ARCHIVE;
    
    // Executables
    if (ext == "exe" || ext == "bin" || ext == "dmg" || ext == "pkg" || 
        ext == "deb" || ext == "rpm" || ext == "appimage" || ext == "so" || 
        ext == "dll" || ext == "dylib") return EXECUTABLE;
    
    // Text files (CSV is handled as SPREADSHEET above)
    if (ext == "txt" || ext == "md" || ext == "markdown" || 
        ext == "log" || ext == "json" || ext == "xml" || ext == "html" || 
        ext == "htm" || ext == "css" || ext == "js" || ext == "ts" || 
        ext == "jsx" || ext == "tsx" || ext == "c" || ext == "cpp" || 
        ext == "cc" || ext == "cxx" || ext == "h" || ext == "hpp" || 
        ext == "hh" || ext == "java" || ext == "py" || ext == "sh" || 
        ext == "bash" || ext == "php" || ext == "rb" || ext == "go" || 
        ext == "rs" || ext == "yaml" || ext == "yml" || ext == "ini" || 
        ext == "conf" || ext == "cfg") return TEXT;
    
    return UNKNOWN;
}

FileTypeDetector::FileType FileTypeDetector::detectFileType(std::string_view filename) {
    return detectFileTypeByExtension(getFileExtension(std::string(filename)));
}

FileTypeDetector::FileType FileTypeDetector::detectFileTypeByContent(const std::vector<uint8_t>& data) {
    if (data.size() < 4) {
        return UNKNOWN;
    }
    
    // PDF
    if (data.size() >= 4 && memcmp(data.data(), "%PDF", 4) == 0) {
        return DOCUMENT;
    }
    
    // PNG
    if (data.size() >= 8 && memcmp(data.data(), "\x89PNG\r\n\x1a\n", 8) == 0) {
        return IMAGE;
    }
    
    // JPEG
    if (data.size() >= 3 && memcmp(data.data(), "\xff\xd8\xff", 3) == 0) {
        return IMAGE;
    }
    
    // GIF
    if (data.size() >= 6 && (memcmp(data.data(), "GIF87a", 6) == 0 || memcmp(data.data(), "GIF89a", 6) == 0)) {
        return IMAGE;
    }
    
    // ZIP (includes JAR, DOCX, XLSX, etc.)
    if (data.size() >= 4 && (memcmp(data.data(), "PK\x03\x04", 4) == 0 || 
                              memcmp(data.data(), "PK\x05\x06", 4) == 0 || 
                              memcmp(data.data(), "PK\x07\x08", 4) == 0)) {
        // Check if it's a spreadsheet (XLSX, ODS)
        // XLSX files are ZIP archives with specific structure
        // For now, treat as archive - extension-based detection is more reliable
        return ARCHIVE;
    }
    
    // ISO 9660 filesystem image (starts with CD001 or CDROM)
    if (data.size() >= 6) {
        if (memcmp(data.data() + 0x8001, "CD001", 5) == 0 || 
            memcmp(data.data() + 0x8801, "CD001", 5) == 0 ||
            memcmp(data.data() + 0x9001, "CD001", 5) == 0) {
            return FS_IMAGE;
        }
    }
    
    // SQLite database (starts with "SQLite format 3")
    if (data.size() >= 16 && memcmp(data.data(), "SQLite format 3", 15) == 0) {
        return DATABASE;
    }
    
    // ELF executable
    if (data.size() >= 4 && data[0] == 0x7f && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') {
        return EXECUTABLE;
    }
    
    // Windows PE executable
    if (data.size() >= 2 && memcmp(data.data(), "MZ", 2) == 0) {
        return EXECUTABLE;
    }
    
    // MP3 (ID3v2 tag)
    if (data.size() >= 3 && memcmp(data.data(), "ID3", 3) == 0) {
        return AUDIO;
    }
    
    // MP4/MOV
    if (data.size() >= 12 && memcmp(data.data() + 4, "ftyp", 4) == 0) {
        return VIDEO;
    }
    
    // Check if it's text
    if (isTextContent(data)) {
        return TEXT;
    }
    
    return UNKNOWN;
}

std::string FileTypeDetector::getMimeTypeByExtension(std::string_view extension) {
    std::string ext(extension);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Text formats
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "txt") return "text/plain";
    if (ext == "json") return "application/json";
    if (ext == "xml") return "application/xml";
    if (ext == "md" || ext == "markdown") return "text/markdown";
    
    // Spreadsheets
    if (ext == "xls") return "application/vnd.ms-excel";
    if (ext == "xlsx") return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    if (ext == "xlsm") return "application/vnd.ms-excel.sheet.macroEnabled.12";
    if (ext == "xlsb") return "application/vnd.ms-excel.sheet.binary.macroEnabled.12";
    if (ext == "ods") return "application/vnd.oasis.opendocument.spreadsheet";
    if (ext == "csv") return "text/csv";
    if (ext == "tsv") return "text/tab-separated-values";
    if (ext == "numbers") return "application/x-iwork-numbers-sffnumbers";
    
    // Database files
    if (ext == "db" || ext == "sqlite" || ext == "sqlite3" || ext == "db3") return "application/x-sqlite3";
    if (ext == "sql") return "application/sql";
    if (ext == "mdb") return "application/x-msaccess";
    if (ext == "accdb") return "application/msaccess";
    if (ext == "dbf") return "application/x-dbf";
    if (ext == "odb") return "application/vnd.oasis.opendocument.database";
    if (ext == "fdb") return "application/x-firebird";
    if (ext == "gdb") return "application/x-gdb";
    
    // Filesystem images
    if (ext == "iso") return "application/x-iso9660-image";
    if (ext == "img") return "application/octet-stream";
    if (ext == "dmg") return "application/x-apple-diskimage";
    if (ext == "vdi") return "application/x-virtualbox-vdi";
    if (ext == "vmdk") return "application/x-virtualbox-vmdk";
    if (ext == "vhd" || ext == "vhdx") return "application/x-virtualbox-vhd";
    if (ext == "qcow2") return "application/x-qemu-disk";
    if (ext == "raw") return "application/octet-stream";
    if (ext == "nrg") return "application/x-nero";
    if (ext == "mdf") return "application/x-msmediaview";
    if (ext == "cdr") return "application/x-cdr";
    if (ext == "toast") return "application/x-toast";
    if (ext == "cso") return "application/x-compressed-iso";
    if (ext == "yaml" || ext == "yml") return "text/yaml";
    if (ext == "ini" || ext == "conf" || ext == "cfg") return "text/plain";
    
    // Code files
    if (ext == "c" || ext == "cpp" || ext == "cc" || ext == "cxx") return "text/x-c++";
    if (ext == "h" || ext == "hpp" || ext == "hh") return "text/x-c++hdr";
    if (ext == "java") return "text/x-java";
    if (ext == "py") return "text/x-python";
    if (ext == "sh" || ext == "bash") return "text/x-shellscript";
    if (ext == "php") return "text/x-php";
    if (ext == "rb") return "text/x-ruby";
    if (ext == "go") return "text/x-go";
    if (ext == "rs") return "text/x-rust";
    if (ext == "ts") return "application/typescript";
    if (ext == "jsx") return "text/jsx";
    if (ext == "tsx") return "text/tsx";
    
    // Images
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "webp") return "image/webp";
    if (ext == "ico") return "image/x-icon";
    if (ext == "bmp") return "image/bmp";
    if (ext == "tiff" || ext == "tif") return "image/tiff";
    
    // Audio
    if (ext == "mp3") return "audio/mpeg";
    if (ext == "wav") return "audio/wav";
    if (ext == "ogg") return "audio/ogg";
    if (ext == "flac") return "audio/flac";
    if (ext == "aac") return "audio/aac";
    if (ext == "m4a") return "audio/mp4";
    
    // Video
    if (ext == "mp4") return "video/mp4";
    if (ext == "avi") return "video/x-msvideo";
    if (ext == "mov") return "video/quicktime";
    if (ext == "webm") return "video/webm";
    if (ext == "mkv") return "video/x-matroska";
    if (ext == "m4v") return "video/mp4";
    
    // Documents
    if (ext == "pdf") return "application/pdf";
    if (ext == "doc") return "application/msword";
    if (ext == "docx") return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (ext == "ppt") return "application/vnd.ms-powerpoint";
    if (ext == "pptx") return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    if (ext == "odt") return "application/vnd.oasis.opendocument.text";
    if (ext == "odp") return "application/vnd.oasis.opendocument.presentation";
    
    // Archives
    if (ext == "zip") return "application/zip";
    if (ext == "tar") return "application/x-tar";
    if (ext == "gz") return "application/gzip";
    if (ext == "bz2") return "application/x-bzip2";
    if (ext == "xz") return "application/x-xz";
    if (ext == "7z") return "application/x-7z-compressed";
    if (ext == "rar") return "application/vnd.rar";
    
    return "application/octet-stream";
}

std::string FileTypeDetector::getMimeType(std::string_view filename) {
    return getMimeTypeByExtension(getFileExtension(filename));
}

std::string FileTypeDetector::getMimeTypeByContent(const std::vector<uint8_t>& data) {
    FileType type = detectFileTypeByContent(data);
    
    // Map detected types to MIME types
    switch (type) {
        case IMAGE:
            if (data.size() >= 8 && memcmp(data.data(), "\x89PNG\r\n\x1a\n", 8) == 0) return "image/png";
            if (data.size() >= 3 && memcmp(data.data(), "\xff\xd8\xff", 3) == 0) return "image/jpeg";
            if (data.size() >= 6 && (memcmp(data.data(), "GIF87a", 6) == 0 || memcmp(data.data(), "GIF89a", 6) == 0)) return "image/gif";
            return "image/*";
        case AUDIO:
            if (data.size() >= 3 && memcmp(data.data(), "ID3", 3) == 0) return "audio/mpeg";
            return "audio/*";
        case VIDEO:
            return "video/*";
        case DOCUMENT:
            if (data.size() >= 4 && memcmp(data.data(), "%PDF", 4) == 0) return "application/pdf";
            return "application/*";
        case ARCHIVE:
            return "application/zip";
        case TEXT:
            return "text/plain";
        default:
            return "application/octet-stream";
    }
}

bool FileTypeDetector::isTextFile(std::string_view filename) {
    return detectFileType(filename) == TEXT;
}

bool FileTypeDetector::isImageFile(std::string_view filename) {
    return detectFileType(filename) == IMAGE;
}

bool FileTypeDetector::isAudioFile(std::string_view filename) {
    return detectFileType(filename) == AUDIO;
}

bool FileTypeDetector::isVideoFile(std::string_view filename) {
    return detectFileType(filename) == VIDEO;
}

bool FileTypeDetector::isDocumentFile(std::string_view filename) {
    return detectFileType(filename) == DOCUMENT;
}

bool FileTypeDetector::isSpreadsheetFile(std::string_view filename) {
    return detectFileType(filename) == SPREADSHEET;
}

bool FileTypeDetector::isDatabaseFile(std::string_view filename) {
    return detectFileType(filename) == DATABASE;
}

bool FileTypeDetector::isFsImageFile(std::string_view filename) {
    return detectFileType(filename) == FS_IMAGE;
}

bool FileTypeDetector::isArchiveFile(std::string_view filename) {
    return detectFileType(filename) == ARCHIVE;
}

bool FileTypeDetector::isExecutableFile(std::string_view filename) {
    return detectFileType(filename) == EXECUTABLE;
}

bool FileTypeDetector::isTextContent(const std::vector<uint8_t>& data) {
    if (data.empty()) return false;
    
    int textChars = 0;
    size_t checkSize = data.size() < 1024 ? data.size() : 1024;
    
    for (size_t i = 0; i < checkSize; ++i) {
        unsigned char c = data[i];
        if (c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c < 127) || (c >= 0xC0 && c < 0xFF)) {
            textChars++;
        }
    }
    
    // If more than 80% are text characters, consider it text
    return checkSize > 0 && (textChars * 100 / checkSize) >= 80;
}

bool FileTypeDetector::isBinaryContent(const std::vector<uint8_t>& data) {
    return !isTextContent(data);
}

std::string FileTypeDetector::getFileDescription(std::string_view filename) {
    FileType type = detectFileType(filename);
    return getFileTypeString(type);
}

std::string FileTypeDetector::getFileTypeString(FileType type) {
    switch (type) {
        case TEXT: return "Text file";
        case IMAGE: return "Image";
        case AUDIO: return "Audio";
        case VIDEO: return "Video";
        case DOCUMENT: return "Document";
        case ARCHIVE: return "Archive";
        case EXECUTABLE: return "Executable";
        case DIRECTORY: return "Directory";
        case SPREADSHEET: return "Spreadsheet";
        case DATABASE: return "Database";
        case FS_IMAGE: return "Filesystem image";
        default: return "Unknown";
    }
}

std::string FileTypeDetector::getIcon(std::string_view filename, bool color) {
    return color ? getColorIcon(filename) : getMonoIcon(filename);
}

std::string FileTypeDetector::getMonoIcon(std::string_view filename) {
    FileType type = detectFileType(filename);
    bool isDir = (type == DIRECTORY);
    
    if (isDir) {
        return getFolderMonoIcon();
    }
    
    std::string ext = getFileExtension(filename);
    
    // Extension-based icon selection for more granular icons
    // Images
    if (type == IMAGE) {
        if (ext == "svg") return m_monoStyle + "code-bracket.svg";
        if (ext == "gif") return m_monoStyle + "photo.svg";
        if (ext == "png" || ext == "jpg" || ext == "jpeg") return m_monoStyle + "photo.svg";
        if (ext == "webp" || ext == "ico" || ext == "bmp") return m_monoStyle + "photo.svg";
        if (ext == "tiff" || ext == "tif") return m_monoStyle + "photo.svg";
        return m_monoStyle + "photo.svg";
    }
    
    // Audio
    if (type == AUDIO) {
        if (ext == "mp3") return m_monoStyle + "musical-note.svg";
        if (ext == "wav" || ext == "flac") return m_monoStyle + "musical-note.svg";
        if (ext == "ogg" || ext == "aac" || ext == "m4a") return m_monoStyle + "musical-note.svg";
        return m_monoStyle + "musical-note.svg";
    }
    
    // Video
    if (type == VIDEO) {
        return m_monoStyle + "video-camera.svg";
    }
    
    // Archives
    if (type == ARCHIVE) {
        if (ext == "zip") return m_monoStyle + "archive-box.svg";
        if (ext == "tar" || ext == "gz" || ext == "bz2" || ext == "xz") return m_monoStyle + "archive-box.svg";
        if (ext == "7z" || ext == "rar") return m_monoStyle + "archive-box.svg";
        return m_monoStyle + "archive-box.svg";
    }
    
    // Spreadsheets
    if (type == SPREADSHEET) {
        if (ext == "xls" || ext == "xlsx" || ext == "xlsm" || ext == "xlsb") return m_monoStyle + "table-cells.svg";
        if (ext == "ods") return m_monoStyle + "table-cells.svg";
        if (ext == "csv" || ext == "tsv") return m_monoStyle + "table-cells.svg";
        return m_monoStyle + "table-cells.svg";
    }
    
    // Databases
    if (type == DATABASE) {
        if (ext == "sqlite" || ext == "sqlite3" || ext == "db" || ext == "db3") return m_monoStyle + "chart-bar.svg";
        return m_monoStyle + "chart-bar.svg";
    }
    
    // Filesystem images
    if (type == FS_IMAGE) {
        if (ext == "iso") return m_monoStyle + "archive-box.svg";
        if (ext == "img" || ext == "dmg") return m_monoStyle + "archive-box.svg";
        if (ext == "vdi" || ext == "vmdk" || ext == "vhd" || ext == "vhdx") return m_monoStyle + "archive-box.svg";
        return m_monoStyle + "archive-box.svg";
    }
    
    // Text files - more granular
    if (type == TEXT) {
        if (ext == "c" || ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "h" || ext == "hpp" || ext == "hh") {
            return m_monoStyle + "code-bracket.svg";
        }
        if (ext == "java") return m_monoStyle + "code-bracket.svg";
        if (ext == "py") return m_monoStyle + "code-bracket.svg";
        if (ext == "js" || ext == "ts" || ext == "jsx" || ext == "tsx") return m_monoStyle + "code-bracket.svg";
        if (ext == "sh" || ext == "bash") return m_monoStyle + "command-line.svg";
        if (ext == "php") return m_monoStyle + "code-bracket.svg";
        if (ext == "rb" || ext == "go" || ext == "rs") return m_monoStyle + "code-bracket.svg";
        if (ext == "html" || ext == "htm") return m_monoStyle + "code-bracket.svg";
        if (ext == "css") return m_monoStyle + "code-bracket.svg";
        if (ext == "json" || ext == "xml" || ext == "yaml" || ext == "yml") return m_monoStyle + "code-bracket.svg";
        if (ext == "md" || ext == "markdown") return m_monoStyle + "document-text.svg";
        if (ext == "txt" || ext == "log" || ext == "ini" || ext == "conf" || ext == "cfg") {
            return m_monoStyle + "document-text.svg";
        }
        return m_monoStyle + "document-text.svg";
    }
    
    // Documents
    if (type == DOCUMENT) {
        if (ext == "pdf") return m_monoStyle + "document.svg";
        if (ext == "doc" || ext == "docx") return m_monoStyle + "document.svg";
        if (ext == "ppt" || ext == "pptx") return m_monoStyle + "document.svg";
        if (ext == "odt" || ext == "odp") return m_monoStyle + "document.svg";
        return m_monoStyle + "document.svg";
    }
    
    // Executables
    if (type == EXECUTABLE) {
        return m_monoStyle + "cpu-chip.svg";
    }
    
    return m_monoStyle + "document.svg";
}

std::string FileTypeDetector::getColorIcon(std::string_view filename) {
    FileType type = detectFileType(filename);
    bool isDir = (type == DIRECTORY);
    
    if (isDir) {
        return getFolderColorIcon();
    }
    
    std::string ext = getFileExtension(filename);
    
    // Extension-based icon selection for more granular icons
    // Images - use pictures-folder-memories instead of edit variant
    if (type == IMAGE) {
        if (ext == "svg") return m_colorStyle + "interface-essential/pictures-folder-memories.svg";
        if (ext == "gif") return m_colorStyle + "interface-essential/pictures-folder-memories.svg";
        if (ext == "png" || ext == "jpg" || ext == "jpeg") return m_colorStyle + "interface-essential/pictures-folder-memories.svg";
        if (ext == "webp" || ext == "ico" || ext == "bmp") return m_colorStyle + "interface-essential/pictures-folder-memories.svg";
        if (ext == "tiff" || ext == "tif") return m_colorStyle + "interface-essential/pictures-folder-memories.svg";
        return m_colorStyle + "interface-essential/pictures-folder-memories.svg";
    }
    
    // Audio
    if (type == AUDIO) {
        if (ext == "mp3") return m_colorStyle + "entertainment/music-note-1.svg";
        if (ext == "wav" || ext == "flac") return m_colorStyle + "entertainment/music-note-1.svg";
        if (ext == "ogg" || ext == "aac" || ext == "m4a") return m_colorStyle + "entertainment/music-note-1.svg";
        return m_colorStyle + "entertainment/music-note-1.svg";
    }
    
    // Video
    if (type == VIDEO) {
        if (ext == "mp4" || ext == "m4v") return m_colorStyle + "computer-devices/webcam-video.svg";
        if (ext == "avi" || ext == "mov") return m_colorStyle + "computer-devices/webcam-video.svg";
        if (ext == "webm" || ext == "mkv") return m_colorStyle + "computer-devices/webcam-video.svg";
        return m_colorStyle + "computer-devices/webcam-video.svg";
    }
    
    // Archives
    if (type == ARCHIVE) {
        if (ext == "zip") return m_colorStyle + "interface-essential/archive-box.svg";
        if (ext == "tar" || ext == "gz" || ext == "bz2" || ext == "xz") return m_colorStyle + "interface-essential/archive-box.svg";
        if (ext == "7z" || ext == "rar") return m_colorStyle + "interface-essential/archive-box.svg";
        return m_colorStyle + "interface-essential/archive-box.svg";
    }
    
    // Spreadsheets
    if (type == SPREADSHEET) {
        if (ext == "xls" || ext == "xlsx" || ext == "xlsm" || ext == "xlsb") return m_colorStyle + "money-shopping/graph.svg";
        if (ext == "ods") return m_colorStyle + "money-shopping/graph.svg";
        if (ext == "csv" || ext == "tsv") return m_colorStyle + "money-shopping/graph.svg";
        return m_colorStyle + "money-shopping/graph.svg";
    }
    
    // Databases
    if (type == DATABASE) {
        if (ext == "sqlite" || ext == "sqlite3" || ext == "db" || ext == "db3") return m_colorStyle + "computer-devices/database.svg";
        if (ext == "sql") return m_colorStyle + "computer-devices/database.svg";
        if (ext == "mdb" || ext == "accdb") return m_colorStyle + "computer-devices/database.svg";
        return m_colorStyle + "computer-devices/database.svg";
    }
    
    // Filesystem images
    if (type == FS_IMAGE) {
        if (ext == "iso") return m_colorStyle + "computer-devices/hard-disk.svg";
        if (ext == "img" || ext == "dmg") return m_colorStyle + "computer-devices/hard-disk.svg";
        if (ext == "vdi" || ext == "vmdk" || ext == "vhd" || ext == "vhdx") return m_colorStyle + "computer-devices/hard-disk.svg";
        return m_colorStyle + "computer-devices/hard-disk.svg";
    }
    
    // Text files - more granular
    if (type == TEXT) {
        if (ext == "c" || ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "h" || ext == "hpp" || ext == "hh") {
            return m_colorStyle + "computer-devices/desktop-code.svg";
        }
        if (ext == "java") return m_colorStyle + "computer-devices/desktop-code.svg";
        if (ext == "py") return m_colorStyle + "computer-devices/desktop-code.svg";
        if (ext == "js" || ext == "ts" || ext == "jsx" || ext == "tsx") return m_colorStyle + "computer-devices/desktop-code.svg";
        if (ext == "sh" || ext == "bash") return m_colorStyle + "computer-devices/desktop-code.svg";
        if (ext == "php") return m_colorStyle + "computer-devices/desktop-code.svg";
        if (ext == "rb" || ext == "go" || ext == "rs") return m_colorStyle + "computer-devices/desktop-code.svg";
        if (ext == "html" || ext == "htm") return m_colorStyle + "computer-devices/desktop-code.svg";
        if (ext == "css") return m_colorStyle + "computer-devices/desktop-code.svg";
        if (ext == "json" || ext == "xml" || ext == "yaml" || ext == "yml") return m_colorStyle + "computer-devices/desktop-code.svg";
        if (ext == "md" || ext == "markdown") return m_colorStyle + "interface-essential/new-file.svg";
        if (ext == "txt" || ext == "log" || ext == "ini" || ext == "conf" || ext == "cfg") {
            return m_colorStyle + "interface-essential/new-file.svg";
        }
        return m_colorStyle + "interface-essential/new-file.svg";
    }
    
    // Documents
    if (type == DOCUMENT) {
        if (ext == "pdf") return m_colorStyle + "interface-essential/new-file.svg";

    }
    
    // Executables
    if (type == EXECUTABLE) {
        return m_colorStyle + "computer-devices/computer-chip-1.svg";
    }
    
    return m_colorStyle + "interface-essential/new-file.svg";
}

std::string FileTypeDetector::getFolderIcon(bool color) {
    return color ? getFolderColorIcon() : getFolderMonoIcon();
}

std::string FileTypeDetector::getFolderMonoIcon() {
    return m_monoStyle + "folder.svg";
}

std::string FileTypeDetector::getFolderColorIcon() {
    return m_colorStyle + "interface-essential/new-folder.svg";
}

bool FileTypeDetector::hasSignature(const std::vector<uint8_t>& data, const std::vector<uint8_t>& signature) {
    if (data.size() < signature.size()) {
        return false;
    }
    return memcmp(data.data(), signature.data(), signature.size()) == 0;
}
