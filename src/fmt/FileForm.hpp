#ifndef FILEFORM_H
#define FILEFORM_H

#include "../volume/VolumeFile.hpp"

class IFileForm {
public:
    virtual ~IFileForm() {}
    
    virtual void persistObject(VolumeFile file) = 0;
    virtual void restoreObject(VolumeFile file) = 0;
};

#endif // FILEFORM_H