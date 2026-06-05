#ifndef BAS_SECURITY_COMMAND_SUPPORT_HPP
#define BAS_SECURITY_COMMAND_SUPPORT_HPP

#include "Realm.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace bas::security {

/** Console command dispatch for demo / REPL integration. */
class ICommandSupport {
  public:
    virtual ~ICommandSupport() = default;

    /** @return process exit code (0 = success). @a args is consumed via shift. */
    virtual int invoke(std::vector<std::string>& args) = 0;

    /**
     * Tab-completion candidates for the word at @a index in @a args.
     * If @a index >= args.size(), the current word is treated as "".
     */
    virtual std::vector<std::string> complete(const std::vector<std::string>& args,
                                              std::size_t index) const = 0;
};

/** Current word for completion; empty when @a index is past the end of @a args. */
std::string currentWord(const std::vector<std::string>& args, std::size_t index);

std::string shiftArg(std::vector<std::string>& args);

std::string takeFlag(std::vector<std::string>& args, const std::string& flag);

bool hasFlag(const std::vector<std::string>& args, const std::string& flag);

/** Remove @a flag if present (no value). Returns whether it was found. */
bool consumeFlag(std::vector<std::string>& args, const std::string& flag);

/** Remove all -h/--help flags from @a args; return true if any were present. */
bool takeHelpRequest(std::vector<std::string>& args);

/** True when @a args is non-empty and every token is -h or --help. */
bool argsAreOnlyHelpFlags(const std::vector<std::string>& args);

int commandSuccess();
int commandFailure();

std::vector<std::string> filterByPrefix(const std::vector<std::string>& options,
                                        const std::string& prefix);

enum class CommandMatch { Found, NotFound, Ambiguous };

struct ResolvedCommand {
    CommandMatch match = CommandMatch::NotFound;
    std::string value;
};

/** Match @a token to exactly one entry in @a commands (exact or unique prefix). */
ResolvedCommand resolveCommandByPrefix(const std::vector<std::string>& commands,
                                       std::string_view token);

bool isKnownRealmType(std::string_view token);

std::optional<Realm> parseAtRealmToken(const std::string& token);

Realm mergeRealm(Realm base, const Realm& overlay);

void writeRealmSuffix(std::ostream& out, const Realm& realm);

} // namespace bas::security

#endif
