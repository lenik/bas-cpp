#include "fslang.hpp"
#include "../volume/Volume.hpp"
#include <sstream>
#include <algorithm>
#include <functional>

namespace fslang {

// Forward declaration
static std::string computeDeepHash(Volume& vol, const std::string& path, std::function<void(const std::string&)> commitCallback);

static std::string executeCommand(Volume& vol, ExecutionContext& ctx, const ASTNode& node, std::function<void()> commitCallback) {
    try {
        if (auto* cmd = std::get_if<CommandMkdir>(&node.command)) {
            std::string path = cmd->path;
            if (path[0] != '/') {
                path = ctx.cwd + "/" + path;
            }
            vol.createDirectory(path);
            if (commitCallback) commitCallback();
            return "";
        }
        else if (auto* cmd = std::get_if<CommandRmdir>(&node.command)) {
            std::string path = cmd->path;
            if (path[0] != '/') {
                path = ctx.cwd + "/" + path;
            }
            vol.removeDirectory(path);
            if (commitCallback) commitCallback();
            return "";
        }
        else if (auto* cmd = std::get_if<CommandCd>(&node.command)) {
            if (cmd->path == "/") {
                ctx.cwd = "/";
            } else if (cmd->path == "..") {
                size_t pos = ctx.cwd.rfind('/');
                if (pos > 0) {
                    ctx.cwd = ctx.cwd.substr(0, pos);
                }
            } else {
                std::string path = cmd->path;
                if (path[0] != '/') {
                    path = ctx.cwd + "/" + path;
                }
                if (vol.exists(path) && vol.isDirectory(path)) {
                    ctx.cwd = path;
                } else {
                    return "cd: directory not found: " + path;
                }
            }
            return "";
        }
        else if (auto* cmd = std::get_if<CommandPut>(&node.command)) {
            std::string path = cmd->path;
            if (path[0] != '/') {
                path = ctx.cwd + "/" + path;
            }
            
            if (auto* text = std::get_if<std::string>(&cmd->content)) {
                vol.writeFileUTF8(path, *text);
            } else if (auto* binary = std::get_if<std::vector<uint8_t>>(&cmd->content)) {
                vol.writeFile(path, *binary);
            }
            if (commitCallback) commitCallback();
            return "";
        }
        else if (auto* cmd = std::get_if<CommandCopy>(&node.command)) {
            std::string src = cmd->src;
            std::string dst = cmd->dst;
            if (src[0] != '/') src = ctx.cwd + "/" + src;
            if (dst[0] != '/') dst = ctx.cwd + "/" + dst;
            vol.copyFile(src, dst);
            if (commitCallback) commitCallback();
            return "";
        }
        else if (auto* cmd = std::get_if<CommandMove>(&node.command)) {
            std::string src = cmd->src;
            std::string dst = cmd->dst;
            if (src[0] != '/') src = ctx.cwd + "/" + src;
            if (dst[0] != '/') dst = ctx.cwd + "/" + dst;
            vol.moveFile(src, dst);
            if (commitCallback) commitCallback();
            return "";
        }
        else if (auto* cmd = std::get_if<CommandRename>(&node.command)) {
            std::string path = cmd->path;
            std::string newname = cmd->newname;
            if (path[0] != '/') path = ctx.cwd + "/" + path;
            
            size_t lastSlash = path.rfind('/');
            std::string parent = (lastSlash != std::string::npos) ? path.substr(0, lastSlash) : "/";
            std::string newPath = (parent == "/") ? ("/" + newname) : (parent + "/" + newname);
            
            vol.rename(path, newPath);
            if (commitCallback) commitCallback();
            return "";
        }
        else if (auto* cmd = std::get_if<CommandCheck>(&node.command)) {
            std::string path = cmd->path;
            if (path[0] != '/') path = ctx.cwd + "/" + path;
            
            if (!vol.exists(path)) {
                return "check: file not found: " + path;
            }
            
            if (auto* expectedText = std::get_if<std::string>(&cmd->content)) {
                std::string actual = vol.readFileUTF8(path);
                if (actual != *expectedText) {
                    return "check: content mismatch at " + path;
                }
            } else if (auto* expectedBinary = std::get_if<std::vector<uint8_t>>(&cmd->content)) {
                auto actual = vol.readFile(path);
                if (actual != *expectedBinary) {
                    return "check: binary content mismatch at " + path;
                }
            }
            return "";
        }
        else if (auto* cmd = std::get_if<CommandDelete>(&node.command)) {
            std::string path = cmd->path;
            if (path[0] != '/') path = ctx.cwd + "/" + path;
            
            if (vol.isDirectory(path)) {
                vol.removeDirectory(path);
            } else {
                vol.removeFile(path);
            }
            if (commitCallback) commitCallback();
            return "";
        }
        else if (auto* cmd = std::get_if<CommandLs>(&node.command)) {
            std::string path = cmd->path;
            if (path[0] != '/') path = ctx.cwd + "/" + path;
            
            auto dir = vol.readDir(path, false);
            std::ostringstream oss;
            for (const auto& [name, child] : dir->children) {
                oss << (child->type == FileType::Directory ? "d" : "-");
                oss << " " << name << "\n";
            }
            ctx.output.push_back(oss.str());
            return "";
        }
        else if (auto* cmd = std::get_if<CommandHash>(&node.command)) {
            std::string hash = computeDeepHash(vol, "/", nullptr);
            if (cmd->var) {
                ctx.variables[*cmd->var] = hash;
            } else {
                ctx.output.push_back("hash: " + hash);
            }
            return "";
        }
        else if (auto* cmd = std::get_if<CommandAssert>(&node.command)) {
            // Simple assertion - just check if condition is non-empty
            if (cmd->condition.empty() || cmd->condition == "false") {
                return "assert: condition failed";
            }
            return "";
        }
        else if (auto* cmd = std::get_if<CommandPrint>(&node.command)) {
            ctx.output.push_back(cmd->message);
            return "";
        }
        
        return "unknown command";
    } catch (const std::exception& e) {
        return std::string("error: ") + e.what();
    }
}

static std::string computeDeepHash(Volume& vol, const std::string& path, std::function<void(const std::string&)> commitCallback) {
    std::ostringstream oss;
    
    try {
        if (vol.isFile(path)) {
            // Hash file content
            auto data = vol.readFile(path);
            oss << "F:" << path << ":" << data.size() << ":";
            for (uint8_t byte : data) {
                oss << std::hex << (int)byte;
            }
        } else if (vol.isDirectory(path)) {
            oss << "D:" << path << ":";
            
            // Get directory entries and sort by name
            auto dir = vol.readDir(path, false);
            std::vector<std::pair<std::string, DirEntry*>> entries;
            for (const auto& [name, child] : dir->children) {
                entries.push_back({name, child.get()});
            }
            std::sort(entries.begin(), entries.end(), 
                [](const auto& a, const auto& b) { return a.first < b.first; });
            
            // Hash children in sorted order (depth-first)
            for (const auto& [name, child] : entries) {
                std::string childPath = (path == "/") ? ("/" + name) : (path + "/" + name);
                oss << computeDeepHash(vol, childPath, commitCallback);
            }
        }
    } catch (...) {
        // Ignore errors in hash computation
    }
    
    return oss.str();
}

ExecutionResult execute(const ASTProgram& program, Volume& vol, std::optional<std::function<void()>> commitCallback) {
    ExecutionResult result;
    result.success = true;
    
    for (const auto& node : program) {
        if (!node) continue;
        
        std::string error = executeCommand(vol, result.context, *node, 
            commitCallback ? *commitCallback : std::function<void()>());
        
        if (!error.empty()) {
            result.success = false;
            result.error = error;
            result.failedLine = node->line;
            break;
        }
    }
    
    return result;
}

} // namespace fslang
