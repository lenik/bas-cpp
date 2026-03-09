#ifndef IMAGEFORMAT_H
#define IMAGEFORMAT_H

#include <cstddef>
#include <string_view>

struct Section {
    size_t offset;
    size_t length;
};

/**
 * Finds sections in executable image (PE, ELF, Mach-O)
 */
class ImageFormat {
public:
    static Section findSection(std::string_view sectionName);
    
private:
    static Section findSectionPE(std::string_view sectionName);
    static Section findSectionELF(std::string_view sectionName);
    static Section findSectionMachO(std::string_view sectionName);
};

#endif // IMAGEFORMAT_H
