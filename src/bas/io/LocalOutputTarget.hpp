#ifndef LOCALOUTPUTTARGET_H
#define LOCALOUTPUTTARGET_H

#include "OutputTarget.hpp"

#include <string>

class LocalOutputTarget : public OutputTarget {
public:
    explicit LocalOutputTarget(const std::string& file,
                               std::string_view charset = "UTF-8");

    std::unique_ptr<OutputStream> newOutputStream(bool append = false) override;
    std::unique_ptr<IWriteStream> newWriter(bool append = false) override;

    URI getURI() const override;
    URL getURL() const override;
    std::string getName() const override;
    std::string getCharset() const override;

private:
    std::string m_file;
    std::string m_charset;
};

#endif // LOCALOUTPUTTARGET_H
