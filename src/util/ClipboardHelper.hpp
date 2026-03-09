#ifndef CLIPBOARDHELPER_H
#define CLIPBOARDHELPER_H

#include <cstdint>
#include <string>
#include <vector>

/**
 * Clipboard utility functions for file operations
 */
class ClipboardHelper {
public:
    enum Operation {
        COPY,
        CUT
    };
    
    // File clipboard operations
    static bool copyFilesToClipboard(const std::vector<std::string>& filePaths);
    static bool cutFilesToClipboard(const std::vector<std::string>& filePaths);
    static std::vector<std::string> getFilesFromClipboard();
    static Operation getClipboardOperation();
    static bool hasFiles();
    static void clearFileClipboard();
    
    // Text clipboard operations
    static bool copyTextToClipboard(const std::string& text);
    static std::string getTextFromClipboard();
    static bool hasText();
    
    // Image clipboard operations
    static bool copyImageToClipboard(const std::vector<uint8_t>& imageData);
    static std::vector<uint8_t> getImageFromClipboard();
    static bool hasImage();
    
    // General clipboard operations
    static void clearClipboard();
    static bool isEmpty();
    
private:
    static std::vector<std::string> s_clipboardFiles;
    static Operation s_clipboardOperation;
    static bool s_hasFiles;
};

#endif // CLIPBOARDHELPER_H
