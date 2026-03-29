#include "ImageFormat.hpp"

#include "proc/UseAssets.hpp"

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/loader.h>
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <elf.h>
#include <link.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#else
#   error "Unsupported platform"
#endif

#include <cstring>
#include <string>
#include <string_view>
#include <string.h>

// Symbols from assets.s (or assets_stub.cpp when no embedded assets); C linkage
// extern "C" const uint8_t bas_cpp_assets_data_start[];
// extern "C" const uint8_t bas_cpp_assets_data_end[];

Section ImageFormat::findSection(std::string_view sectionName) {
//, const uint8_t* start, const uint8_t* end) {
    // if (start == nullptr || end == nullptr) {
    //     return {0, 0};
    // }
    // For "assets" section, use the symbols from assets.s
    // if (sectionName == "assets") {
    //     Section section;
    //     section.offset = reinterpret_cast<size_t>(start);
    //     section.length = end - start;
    //     return section;
    // }
    
    // For other sections, use platform-specific methods
#ifdef _WIN32
    return findSectionPE(sectionName);
#elif defined(__APPLE__)
    return findSectionMachO(sectionName);
#else
    return findSectionELF(sectionName);
#endif
}

#ifdef _WIN32
Section ImageFormat::findSectionPE(std::string_view sectionName) {
    Section section = {0, 0};
    
    HMODULE hModule = GetModuleHandle(NULL);
    if (!hModule) {
        return section;
    }
    
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)hModule;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return section;
    }
    
    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)((char*)hModule + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return section;
    }
    
    std::string nameStr(sectionName);
    IMAGE_SECTION_HEADER* sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        char name[9] = {0};
        strncpy(name, (char*)sectionHeader[i].Name, 8);
        if (strcmp(name, nameStr.c_str()) == 0) {
            section.offset = (size_t)hModule + sectionHeader[i].VirtualAddress;
            section.length = sectionHeader[i].Misc.VirtualSize;
            break;
        }
    }
    
    return section;
}
#elif defined(__APPLE__)
Section ImageFormat::findSectionMachO(std::string_view sectionName) {
    Section section = {0, 0};
    
    std::string nameStr(sectionName);
    // Get the main executable path
    uint32_t imageCount = _dyld_image_count();
    for (uint32_t i = 0; i < imageCount; ++i) {
        const struct mach_header* header = (const struct mach_header*)_dyld_get_image_header(i);
        if (header->magic == MH_MAGIC_64 || header->magic == MH_MAGIC) {
            // Check if this is the main executable
            const char* imageName = _dyld_get_image_name(i);
            if (imageName && strstr(imageName, "SecureFileExplorer") != nullptr) {
                // Parse Mach-O structure
                uintptr_t slide = _dyld_get_image_vmaddr_slide(i);
                const uint8_t* base = (const uint8_t*)header;
                
                if (header->magic == MH_MAGIC_64) {
                    const struct mach_header_64* header64 = (const struct mach_header_64*)header;
                    const struct load_command* cmd = (const struct load_command*)(base + sizeof(struct mach_header_64));
                    
                    for (uint32_t j = 0; j < header64->ncmds; ++j) {
                        if (cmd->cmd == LC_SEGMENT_64) {
                            const struct segment_command_64* seg = (const struct segment_command_64*)cmd;
                            if (strcmp(seg->segname, "__DATA") == 0 || strcmp(seg->segname, "__TEXT") == 0) {
                                const struct section_64* sect = (const struct section_64*)((const uint8_t*)seg + sizeof(struct segment_command_64));
                                for (uint32_t k = 0; k < seg->nsects; ++k) {
                                    if (strcmp(sect[k].sectname, nameStr.c_str()) == 0) {
                                        section.offset = (size_t)(sect[k].addr + slide);
                                        section.length = sect[k].size;
                                        return section;
                                    }
                                }
                            }
                        }
                        cmd = (const struct load_command*)((const uint8_t*)cmd + cmd->cmdsize);
                    }
                }
            }
        }
    }
    
    return section;
}
#elif defined(__linux__)
// ELF implementation
struct ElfSearchData {
    std::string name;
    Section* result;
};

static int findElfSectionCallback(struct dl_phdr_info* info, size_t size, void* data) {
    (void)size; // Unused parameter
    ElfSearchData* searchData = static_cast<ElfSearchData*>(data);
    
    // Only process the main executable
    if (info->dlpi_name && strlen(info->dlpi_name) > 0) {
        return 0;
    }
    
    // Open and parse the ELF file
    int fd = open("/proc/self/exe", O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    
    // Read ELF header
    Elf64_Ehdr ehdr;
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        close(fd);
        return 0;
    }
    
    // Verify ELF magic
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        close(fd);
        return 0;
    }
    
    // Read section headers
    lseek(fd, ehdr.e_shoff, SEEK_SET);
    
    // Read section name string table
    lseek(fd, ehdr.e_shoff + ehdr.e_shstrndx * sizeof(Elf64_Shdr), SEEK_SET);
    Elf64_Shdr shstrtab;
    read(fd, &shstrtab, sizeof(shstrtab));
    
    char* shstrtab_data = new char[shstrtab.sh_size];
    lseek(fd, shstrtab.sh_offset, SEEK_SET);
    read(fd, shstrtab_data, shstrtab.sh_size);
    
    // Iterate through sections
    lseek(fd, ehdr.e_shoff, SEEK_SET);
    for (int i = 0; i < ehdr.e_shnum; ++i) {
        Elf64_Shdr shdr;
        if (read(fd, &shdr, sizeof(shdr)) != sizeof(shdr)) {
            break;
        }
        
        const char* sectionName = shstrtab_data + shdr.sh_name;
        if (strcmp(sectionName, searchData->name.c_str()) == 0) {
            // Found the section - get its virtual address
            // For loaded sections, we need to find the corresponding program header
            lseek(fd, ehdr.e_phoff, SEEK_SET);
            for (int j = 0; j < ehdr.e_phnum; ++j) {
                Elf64_Phdr phdr;
                if (read(fd, &phdr, sizeof(phdr)) != sizeof(phdr)) {
                    break;
                }
                
                if (phdr.p_type == PT_LOAD && 
                    shdr.sh_offset >= phdr.p_offset && 
                    shdr.sh_offset < phdr.p_offset + phdr.p_filesz) {
                    // Calculate virtual address
                    size_t baseAddr = info->dlpi_addr;
                    searchData->result->offset = baseAddr + phdr.p_vaddr + (shdr.sh_offset - phdr.p_offset);
                    searchData->result->length = shdr.sh_size;
                    delete[] shstrtab_data;
                    close(fd);
                    return 1; // Stop iteration
                }
            }
        }
    }
    
    delete[] shstrtab_data;
    close(fd);
    return 0;
}

Section ImageFormat::findSectionELF(std::string_view sectionName) {
    Section section = {0, 0};
    
    ElfSearchData searchData = {std::string(sectionName), &section};
    
    dl_iterate_phdr(findElfSectionCallback, &searchData);
    
    return section;
}
#else
#   error "Unsupported platform"
#endif
