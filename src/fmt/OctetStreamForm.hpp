#ifndef OCTETSTREAMFORM_H
#define OCTETSTREAMFORM_H

#include "FileForm.hpp"

#include "../io/InputStream.hpp"
#include "../io/OutputStream.hpp"

class IOctetStreamForm : public IFileForm {
public:
    virtual ~IOctetStreamForm() {}
    
    void persistObject(VolumeFile file) override {
        auto out = file.newOutputStream();
        if (!out)
            return;
        writeObject(out.get());
        out->close();
    }

    void restoreObject(VolumeFile file) override {
        auto in = file.newInputStream();
        if (!in)
            return;
        readObject(in.get());
        in->close();
    }
    
public:
    virtual void readObject(InputStream* in) = 0;
    virtual void writeObject(OutputStream* out) = 0;
};

#endif // OCTETSTREAMFORM_H