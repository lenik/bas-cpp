#ifndef FILEUTILITY_H
#define FILEUTILITY_H

#include "FileForm.hpp"

#include "../script/property_support.hpp"
#include "../volume/VolumeFile.hpp"

#include <string>
#include <vector>

class IFileUtility : public virtual IFileForm {
public:
    IFileUtility();
    virtual ~IFileUtility();
    
    virtual std::vector<std::string> getSupportedFileExtensions() const = 0;
    
    virtual bool isSupportedFileExtension(const std::string& ext) const;

    virtual bool checkFile(VolumeFile file) const;
    
    const VolumeFile& getFile() const { return file.get(); }
    VolumeFile& getFile() { return file.get(); }

    void setFile(const VolumeFile& file) { this->file.set(file); }
    
    void clearFile() { file.set(VolumeFile()); }

    bool isFileSet() const {
        return !file->isEmpty();
    }

    std::string getFilePath() const;
    void setFilePath(std::string_view filePath);

public:
    virtual void resetObject() = 0;

    virtual void reloadFile();

    // void persistObject(VolumeFile file) override;
    // void restoreObject(VolumeFile file) override;

protected:

public:
    observable<VolumeFile> file;
};

#endif // FILEUTILITY_H