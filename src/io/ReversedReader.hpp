#ifndef REVERSEDREADER_H
#define REVERSEDREADER_H

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using read_backward_func_t = std::function<size_t(uint8_t* buf, size_t end, size_t len)>;

typedef struct _block_t {
    void* data;
    const uint8_t* data_view;
    size_t size;
    struct _block_t *next;
    struct _block_t *prev;
} block_t;

class ReversedReader {
private:
    read_backward_func_t m_read_backward_func;
    const size_t m_block_size;
    bool m_optim = false;
    block_t *m_head;
    block_t *m_buf;
    size_t m_bufptr;

public:
    ReversedReader(read_backward_func_t read_backward_func, size_t block_size = 4096);
    ~ReversedReader();

    block_t *next(block_t *block);
    block_t* fetch_block();

    // read single char from backward, -1 for EOF
    int read();

    // return actual bytes read, 0 for EOF
    size_t read(uint8_t* buf, size_t end, size_t len);

    bool reject();

    bool reject(size_t n);
    // since this is reversed reader, readAhead is actually look backward
    int lookAhead(uint8_t* buf, size_t end, size_t len);

    // since this is reversed reader, lookAhead is actually look backward
    int lookAhead();

    // return line (including EOL), empty string for EOF
    std::string readRawLine();

    std::vector<std::string> readRawLines(int maxLines = -1);
    
    std::string readLine(std::string_view encoding = "UTF-8");

    std::vector<std::string> readLines(std::string_view encoding = "UTF-8", int maxLines = -1);

};

#endif // REVERSEDREADER_H