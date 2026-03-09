#include "FileUtility.hpp"

#include "OctetStreamForm.hpp"
#include "TextForm.hpp"

#include <vector>

IFileUtility::IFileUtility()
    : file()
{
    file.connect([this](const VolumeFile& file, const VolumeFile& old_file) {
        if (file.isEmpty()) {
            resetObject();
            return;
        }
        restoreObject(file);
    });
}

IFileUtility::~IFileUtility()
{
    file.disconnect();
}

bool IFileUtility::isSupportedFileExtension(const std::string& ext) const
{
    auto exts = getSupportedFileExtensions();
    return exts.empty() || std::find(exts.begin(), exts.end(), ext) != exts.end();
}

bool IFileUtility::checkFile(VolumeFile file) const {
    if (file.isEmpty())
        return false;
    if (!file.exists() || !file.isFile())
        return false;
    if (!file.canRead())
        return false;

    std::string path = file.getPath();
    size_t slash = path.rfind('/');
    std::string base = (slash != std::string::npos) ? path.substr(slash + 1) : path;

    size_t dot = base.rfind('.');
    std::string ext =
        (dot != std::string::npos && dot + 1 < base.size()) ? base.substr(dot + 1) : "";
    for (char &c : ext)
        c = std::tolower(c);

    auto fileUtility = dynamic_cast<const IFileUtility *>(this);
    if (!fileUtility)
        return false;
    return this->isSupportedFileExtension(ext);
}

std::string IFileUtility::getFilePath() const
{
    return isFileSet() ? file->getPath() : "";
}

void IFileUtility::setFilePath(std::string_view filePath)
{
    assert(!filePath.empty());
    VolumeFile vf(file->getVolume(), std::string(filePath));
    setFile(vf);
}

void IFileUtility::reloadFile()
{
    resetObject();
    if (file->isNotEmpty())
        restoreObject(file.get());
}
