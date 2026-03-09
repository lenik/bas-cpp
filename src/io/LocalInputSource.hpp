#ifndef LOCALINPUTSOURCE_H
#define LOCALINPUTSOURCE_H

#include "InputSource.hpp"

#include <string>

class LocalInputSource : public DecodingInputSource {
public:
    explicit LocalInputSource(const std::string& file,
                              std::string_view charset = "UTF-8");

    std::unique_ptr<InputStream> newInputStream() override;
    std::unique_ptr<RandomInputStream> newRandomInputStream() override;

    URI getURI() const override;
    URL getURL() const override;
    std::string getName() const override;
    std::string getCharset() const override;

private:
    std::string m_file;
    std::string m_charset;
};

#endif // LOCALINPUTSOURCE_H
