#ifndef URLOUTPUTTARGET_H
#define URLOUTPUTTARGET_H

#include "URL.hpp"

#include "../io/OutputTarget.hpp"

#include <string>

/** OutputTarget that opens a URL via NetProtocol (file, http, https, ftp, etc.). */
class URLOutputTarget : public OutputTarget {
public:
    explicit URLOutputTarget(const URL& url, std::string_view charset = "UTF-8");
    explicit URLOutputTarget(URL&& url, std::string_view charset = "UTF-8");

    std::unique_ptr<OutputStream> newOutputStream(bool append = false) override;
    std::unique_ptr<IWriteStream> newWriter(bool append = false) override;

    URI getURI() const override;
    URL getURL() const override;
    std::string getName() const override;
    std::string getCharset() const override;

private:
    URL m_url;
    std::string m_charset;
};

#endif // URLOUTPUTTARGET_H
