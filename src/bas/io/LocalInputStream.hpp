#ifndef LOCALINPUTSTREAM_H
#define LOCALINPUTSTREAM_H

#include "InputStream.hpp"

#include <cstdint>
#include <fstream>
#include <ios>

class LocalInputStream : public RandomInputStream {
public:
    explicit LocalInputStream(const std::string& path);
    ~LocalInputStream();

    int read() override;
    size_t read(uint8_t* buf, size_t off, size_t len) override;

    int64_t skip(int64_t len) override;

    int64_t position() const override;
    bool seek(int64_t offset, std::ios::seekdir dir = std::ios::beg) override;

    void close() override;
    bool isOpen() const { return m_file.is_open(); }

private:
    mutable std::ifstream m_file;
};

#endif // LOCALINPUTSTREAM_H

