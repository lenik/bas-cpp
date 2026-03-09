#ifndef TEXTFORM_H
#define TEXTFORM_H

#include "FileForm.hpp"

#include "../io/Reader.hpp"
#include "../io/Writer.hpp"

class ITextForm : public IFileForm {
public:
    ITextForm() = default;
    virtual ~ITextForm() {}

    void persistObject(VolumeFile file) override {
        auto out = file.newWriter();
        if (!out)
            return;
        writeObject(out.get());
        out->close();
    }

    void restoreObject(VolumeFile file) override {
        auto in = file.newReader();
        if (!in)
            return;
        readObject(in.get());
        in->close();
    }
    
public:
    virtual void readObject(Reader* in) = 0;
    virtual void writeObject(Writer* out) = 0;
};

#endif // TEXTFORM_H