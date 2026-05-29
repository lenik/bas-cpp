#ifndef REPL_HPP
#define REPL_HPP

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace bas::util::repl {

using CompleteFn =
    std::function<std::vector<std::string>(const std::vector<std::string>&, std::size_t)>;

enum class ReadLineStatus {
    Submitted,
    Pop,
    Eof,
};

/** Split @a line at @a cursor into completion tokens; @a index is the active word. */
void parseCommandLine(const std::string& line, std::size_t cursor, std::vector<std::string>& args,
                      std::size_t& index);

/**
 * Read one input line with tab completion and history (up/down).
 * When @a popOnEmptyEof is true, Ctrl-D on an empty line returns Pop instead of Eof.
 * Appends non-empty submitted lines to @a history.
 */
ReadLineStatus readLine(const std::string& prompt, const CompleteFn& complete,
                        std::vector<std::string>& history, std::string& out,
                        bool popOnEmptyEof = false);

} // namespace bas::util::repl

#endif
