#ifndef FILEUTILITY_H
#define FILEUTILITY_H

#include "FileForm.hpp"

#include "../script/property_support.hpp"
#include "../volume/VolumeFile.hpp"

#include <optional>
#include <string>
#include <vector>

class IFileUtility : public virtual IFileForm {
public:
    IFileUtility();
    virtual ~IFileUtility();
    
    virtual std::vector<std::string> getSupportedFileExtensions() const = 0;
    
    virtual bool isSupportedFileExtension(const std::string& ext) const;

    virtual bool checkFile(VolumeFile file) const;
    
    const std::optional<VolumeFile>& getFile() const { return file.get(); }
    std::optional<VolumeFile>& getFile() { return file.get(); }

    void setFile(const VolumeFile& vf) { this->file.set(vf); }
    void setFile(VolumeFile&& vf) { this->file.set(std::move(vf)); }
    void setFile(std::optional<VolumeFile> vf) { this->file.set(std::move(vf)); }
    
    void clearFile() { file.set(std::nullopt); }

    bool isFileSet() const {
        return file->has_value();
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
    observable<std::optional<VolumeFile>> file;
};

#endif // FILEUTILITY_H