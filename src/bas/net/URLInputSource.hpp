#ifndef URLINPUTSOURCE_H
#define URLINPUTSOURCE_H

#include "URL.hpp"

#include "../io/InputSource.hpp"

#include <string>

/** InputSource that opens a URL via NetProtocol (file, http, https, ftp, etc.). */
class URLInputSource : public DecodingInputSource {
public:
    explicit URLInputSource(const URL& url, std::string_view charset = "UTF-8");
    explicit URLInputSource(URL&& url, std::string_view charset = "UTF-8");

    URI getURI() const override;
    URL getURL() const override;
    std::string getName() const override;
    std::string getCharset() const override;

    std::unique_ptr<InputStream> newInputStream() override;
    std::unique_ptr<RandomInputStream> newRandomInputStream() override;

private:
    URL m_url;
    std::string m_charset;
};

#endif // URLINPUTSOURCE_H
