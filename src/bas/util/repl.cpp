#include "repl.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <termios.h>
#include <unistd.h>
#endif

namespace bas::util::repl {

bool isSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

std::size_t cursorWordLeft(const std::string& line, std::size_t cursor) {
    if (cursor == 0) {
        return 0;
    }
    std::size_t pos = cursor - 1;
    while (pos > 0 && isSpace(line[pos])) {
        --pos;
    }
    while (pos > 0 && !isSpace(line[pos - 1])) {
        --pos;
    }
    return pos;
}

std::size_t cursorWordRight(const std::string& line, std::size_t cursor) {
    std::size_t pos = cursor;
    while (pos < line.size() && isSpace(line[pos])) {
        ++pos;
    }
    while (pos < line.size() && !isSpace(line[pos])) {
        ++pos;
    }
    return pos;
}

void moveCursorLeft(std::size_t& cursor) {
    if (cursor > 0) {
        --cursor;
    }
}

void moveCursorRight(const std::string& line, std::size_t& cursor) {
    if (cursor < line.size()) {
        ++cursor;
    }
}

void moveCursorWordLeft(const std::string& line, std::size_t& cursor) {
    cursor = cursorWordLeft(line, cursor);
}

void moveCursorWordRight(const std::string& line, std::size_t& cursor) {
    cursor = cursorWordRight(line, cursor);
}

void killToEnd(std::string& line, std::size_t& cursor) { line.erase(cursor); }

void killToStart(std::string& line, std::size_t& cursor) {
    line.erase(0, cursor);
    cursor = 0;
}

void killWordBackward(std::string& line, std::size_t& cursor) {
    const std::size_t next = cursorWordLeft(line, cursor);
    line.erase(next, cursor - next);
    cursor = next;
}

void killWordForward(std::string& line, std::size_t& cursor) {
    const std::size_t next = cursorWordRight(line, cursor);
    line.erase(cursor, next - cursor);
}

void deleteForward(std::string& line, std::size_t& cursor) {
    if (cursor < line.size()) {
        line.erase(cursor, 1);
    }
}

std::string longestCommonPrefix(const std::vector<std::string>& options) {
    if (options.empty()) {
        return {};
    }
    std::string prefix = options.front();
    for (std::size_t i = 1; i < options.size(); ++i) {
        const std::string& option = options[i];
        std::size_t len = 0;
        const std::size_t maxLen = std::min(prefix.size(), option.size());
        while (len < maxLen && prefix[len] == option[len]) {
            ++len;
        }
        prefix.resize(len);
        if (prefix.empty()) {
            break;
        }
    }
    return prefix;
}

void redrawLine(const std::string& prompt, const std::string& line, std::size_t cursor) {
    std::cout << "\r" << prompt << line << "\033[K";
    if (cursor < line.size()) {
        std::cout << "\033[" << (line.size() - cursor) << 'D';
    }
    std::cout << std::flush;
}

#if defined(__unix__) || defined(__APPLE__)

bool readChar(unsigned char& ch) { return ::read(STDIN_FILENO, &ch, 1) == 1; }

bool readEscSeq(std::string& seq) {
    unsigned char ch = 0;
    if (!readChar(ch)) {
        return false;
    }
    seq.push_back(static_cast<char>(ch));

    if (ch == 'O') {
        if (!readChar(ch)) {
            return false;
        }
        seq.push_back(static_cast<char>(ch));
        return true;
    }

    if (ch != '[') {
        return true;
    }

    while (readChar(ch)) {
        seq.push_back(static_cast<char>(ch));
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '~') {
            break;
        }
    }
    return true;
}

bool isBackwardKillWordSeq(const std::string& seq) {
    return seq.size() == 1 &&
           (static_cast<unsigned char>(seq[0]) == 127 || static_cast<unsigned char>(seq[0]) == 8);
}

bool isForwardWordSeq(const std::string& seq) {
    return seq == "f" || seq == "F" || seq == "[1;5C" || seq == "[5C" || seq == "[1;3C" ||
           seq == "Oc" || seq == "[1;9C";
}

bool isBackwardWordSeq(const std::string& seq) {
    return seq == "b" || seq == "B" || seq == "[1;5D" || seq == "[5D" || seq == "[1;3D" ||
           seq == "Od" || seq == "[1;9D";
}

bool isHomeSeq(const std::string& seq) {
    return seq == "[H" || seq == "[1~" || seq == "OH" || seq == "[7~";
}

bool isEndSeq(const std::string& seq) {
    return seq == "[F" || seq == "[4~" || seq == "OF" || seq == "[8~";
}

bool handleEscapeSequence(const std::string& seq, const std::string& prompt, std::string& line,
                          std::size_t& cursor, const std::vector<std::string>& history,
                          int& historyPos) {
    if (seq == "[A") { // up
        if (!history.empty() && historyPos > 0) {
            --historyPos;
            line = history[static_cast<std::size_t>(historyPos)];
            cursor = line.size();
            redrawLine(prompt, line, cursor);
        }
        return true;
    }
    if (seq == "[B") { // down
        if (historyPos < static_cast<int>(history.size()) - 1) {
            ++historyPos;
            line = history[static_cast<std::size_t>(historyPos)];
            cursor = line.size();
            redrawLine(prompt, line, cursor);
        } else if (historyPos == static_cast<int>(history.size()) - 1) {
            historyPos = static_cast<int>(history.size());
            line.clear();
            cursor = 0;
            redrawLine(prompt, line, cursor);
        }
        return true;
    }
    if (seq == "[C") { // right
        moveCursorRight(line, cursor);
        redrawLine(prompt, line, cursor);
        return true;
    }
    if (seq == "[D") { // left
        moveCursorLeft(cursor);
        redrawLine(prompt, line, cursor);
        return true;
    }
    if (seq == "[3~") { // delete
        deleteForward(line, cursor);
        redrawLine(prompt, line, cursor);
        return true;
    }
    if (isHomeSeq(seq)) {
        cursor = 0;
        redrawLine(prompt, line, cursor);
        return true;
    }
    if (isEndSeq(seq)) {
        cursor = line.size();
        redrawLine(prompt, line, cursor);
        return true;
    }
    if (isForwardWordSeq(seq)) {
        moveCursorWordRight(line, cursor);
        redrawLine(prompt, line, cursor);
        return true;
    }
    if (isBackwardWordSeq(seq)) {
        moveCursorWordLeft(line, cursor);
        redrawLine(prompt, line, cursor);
        return true;
    }
    if (seq == "d" || seq == "D") { // Alt-D
        killWordForward(line, cursor);
        redrawLine(prompt, line, cursor);
        return true;
    }
    if (isBackwardKillWordSeq(seq)) { // Alt-Backspace
        killWordBackward(line, cursor);
        redrawLine(prompt, line, cursor);
        return true;
    }
    return false;
}

ReadLineStatus readLineRaw(const std::string& prompt, const CompleteFn& complete,
                           std::vector<std::string>& history, std::string& out,
                           bool popOnEmptyEof) {
    termios oldState{};
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &oldState) != 0) {
        std::cout << prompt << std::flush;
        if (!std::getline(std::cin, out)) {
            return ReadLineStatus::Eof;
        }
        if (!out.empty()) {
            history.push_back(out);
        }
        return ReadLineStatus::Submitted;
    }

    termios newState = oldState;
    newState.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    newState.c_cc[VMIN] = 1;
    newState.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newState);

    std::string line;
    std::size_t cursor = 0;
    int historyPos = static_cast<int>(history.size());

    redrawLine(prompt, line, cursor);

    auto applyCompletion = [&](const std::vector<std::string>& candidates) {
        if (candidates.empty()) {
            std::cout << '\a' << std::flush;
            return;
        }
        std::vector<std::string> args;
        std::size_t wordIndex = 0;
        parseCommandLine(line, cursor, args, wordIndex);
        const std::string cword = wordIndex < args.size() ? args[wordIndex] : std::string{};

        std::size_t wordStart = 0;
        std::size_t pos = 0;
        std::size_t current = 0;
        while (pos <= line.size()) {
            while (pos < line.size() && isSpace(line[pos])) {
                ++pos;
            }
            if (current == wordIndex) {
                wordStart = pos;
                break;
            }
            if (pos >= line.size()) {
                wordStart = line.size();
                break;
            }
            while (pos < line.size() && !isSpace(line[pos])) {
                ++pos;
            }
            ++current;
        }
        const std::size_t wordEnd =
            wordIndex < args.size() ? wordStart + args[wordIndex].size() : cursor;

        if (candidates.size() == 1) {
            const std::string& match = candidates.front();
            const std::string suffix =
                match.size() > cword.size() ? match.substr(cword.size()) : "";
            line.replace(wordStart, wordEnd - wordStart, match);
            cursor = wordStart + match.size();
            redrawLine(prompt, line, cursor);
            return;
        }

        const std::string lcp = longestCommonPrefix(candidates);
        if (lcp.size() > cword.size()) {
            line.replace(wordStart, wordEnd - wordStart, lcp);
            cursor = wordStart + lcp.size();
            redrawLine(prompt, line, cursor);
            return;
        }

        std::cout << '\n';
        for (const auto& candidate : candidates) {
            std::cout << candidate << '\n';
        }
        redrawLine(prompt, line, cursor);
        std::cout << '\a' << std::flush;
    };

    while (true) {
        unsigned char ch = 0;
        if (!readChar(ch)) {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldState);
            std::cout << '\n';
            return ReadLineStatus::Eof;
        }

        if (ch == '\n' || ch == '\r') {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldState);
            std::cout << '\n';
            out = line;
            if (!out.empty() && (history.empty() || history.back() != out)) {
                history.push_back(out);
            }
            return ReadLineStatus::Submitted;
        }
        if (ch == 4) { // Ctrl-D
            if (line.empty()) {
                tcsetattr(STDIN_FILENO, TCSANOW, &oldState);
                std::cout << '\n';
                return popOnEmptyEof ? ReadLineStatus::Pop : ReadLineStatus::Eof;
            }
            deleteForward(line, cursor);
            redrawLine(prompt, line, cursor);
            continue;
        }
        if (ch == 1) { // Ctrl-A
            cursor = 0;
            redrawLine(prompt, line, cursor);
            continue;
        }
        if (ch == 2) { // Ctrl-B
            moveCursorLeft(cursor);
            redrawLine(prompt, line, cursor);
            continue;
        }
        if (ch == 5) { // Ctrl-E
            cursor = line.size();
            redrawLine(prompt, line, cursor);
            continue;
        }
        if (ch == 6) { // Ctrl-F
            moveCursorRight(line, cursor);
            redrawLine(prompt, line, cursor);
            continue;
        }
        if (ch == 11) { // Ctrl-K
            killToEnd(line, cursor);
            redrawLine(prompt, line, cursor);
            continue;
        }
        if (ch == 12) { // Ctrl-L
            std::cout << "\033[2J\033[H" << prompt << line << "\033[K";
            if (cursor < line.size()) {
                std::cout << "\033[" << (line.size() - cursor) << 'D';
            }
            std::cout << std::flush;
            continue;
        }
        if (ch == 21) { // Ctrl-U
            killToStart(line, cursor);
            redrawLine(prompt, line, cursor);
            continue;
        }
        if (ch == 23) { // Ctrl-W
            killWordBackward(line, cursor);
            redrawLine(prompt, line, cursor);
            continue;
        }
        if (ch == 3) { // Ctrl-C
            tcsetattr(STDIN_FILENO, TCSANOW, &oldState);
            std::cout << "^C\n";
            out.clear();
            return ReadLineStatus::Submitted;
        }
        if (ch == '\t') {
            if (complete) {
                std::vector<std::string> args;
                std::size_t wordIndex = 0;
                parseCommandLine(line, cursor, args, wordIndex);
                applyCompletion(complete(args, wordIndex));
            } else {
                std::cout << '\a' << std::flush;
            }
            continue;
        }
        if (ch == 127 || ch == 8) {
            if (cursor > 0) {
                line.erase(cursor - 1, 1);
                --cursor;
                redrawLine(prompt, line, cursor);
            }
            continue;
        }
        if (ch == 27) {
            std::string seq;
            if (!readEscSeq(seq)) {
                continue;
            }
            if (handleEscapeSequence(seq, prompt, line, cursor, history, historyPos)) {
                continue;
            }
            continue;
        }
        if (std::iscntrl(static_cast<unsigned char>(ch))) {
            continue;
        }
        line.insert(cursor, 1, static_cast<char>(ch));
        ++cursor;
        redrawLine(prompt, line, cursor);
    }
}

#endif

void parseCommandLine(const std::string& line, std::size_t cursor, std::vector<std::string>& args,
                      std::size_t& index) {
    args.clear();
    index = 0;

    if (cursor > line.size()) {
        cursor = line.size();
    }

    std::size_t pos = 0;
    while (pos <= line.size()) {
        while (pos < line.size() && isSpace(line[pos])) {
            if (pos == cursor) {
                index = args.size();
                return;
            }
            ++pos;
        }
        if (pos == cursor) {
            index = args.size();
            return;
        }
        if (pos >= line.size()) {
            index = args.size();
            return;
        }

        const std::size_t wordStart = pos;
        while (pos < line.size() && !isSpace(line[pos])) {
            ++pos;
        }
        if (cursor >= wordStart && cursor <= pos) {
            args.push_back(line.substr(wordStart, cursor - wordStart));
            index = args.size() - 1;
            return;
        }
        args.push_back(line.substr(wordStart, pos - wordStart));
    }
    index = args.size();
}

ReadLineStatus readLine(const std::string& prompt, const CompleteFn& complete,
                        std::vector<std::string>& history, std::string& out, bool popOnEmptyEof) {
#if defined(__unix__) || defined(__APPLE__)
    return readLineRaw(prompt, complete, history, out, popOnEmptyEof);
#else
    (void)popOnEmptyEof;
    (void)complete;
    std::cout << prompt << std::flush;
    if (!std::getline(std::cin, out)) {
        return ReadLineStatus::Eof;
    }
    if (!out.empty() && (history.empty() || history.back() != out)) {
        history.push_back(out);
    }
    return ReadLineStatus::Submitted;
#endif
}

} // namespace bas::util::repl
