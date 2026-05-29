#include "ac_list.hpp"

#include "json.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace bas::security {

void DefaultACList::add(const ACEntry& entry) { m_entries.push_back(entry); }

void DefaultACList::remove(const ACEntry& entry) {
    m_entries.erase(std::remove(m_entries.begin(), m_entries.end(), entry), m_entries.end());
}

void DefaultACList::clear() { m_entries.clear(); }

const std::vector<ACEntry>& DefaultACList::entries() const { return m_entries; }

DecoratedACList::DecoratedACList(VariantPtr<ACList> wrapped) : m_wrapped(std::move(wrapped)) {}

void DecoratedACList::add(const ACEntry& entry) { m_wrapped->add(entry); }

void DecoratedACList::remove(const ACEntry& entry) { m_wrapped->remove(entry); }

void DecoratedACList::clear() { m_wrapped->clear(); }

const std::vector<ACEntry>& DecoratedACList::entries() const { return m_wrapped->entries(); }

std::vector<ACRule> ACList::rulesOf(const IdentityRef& identity) const {
    std::vector<ACRule> rules;
    for (const auto& entry : entries()) {
        if (entry.identity == identity) {
            rules.push_back(entry.rule);
        }
    }
    return rules;
}

std::vector<ACMatch> ACList::find(const IdentityRef& identity, const Permission& permission,
                                  const PermissionMatcher& matcher) const {
    std::vector<ACMatch> matches;
    for (const auto& entry : entries()) {
        if (entry.identity != identity) {
            continue;
        }
        if (!matcher.matches(entry.rule.permission, permission)) {
            continue;
        }
        ACMatch match;
        match.entry = entry;
        match.permissionSpecificity = matcher.specificity(entry.rule.permission);
        match.identitySpecificity = 10;
        matches.push_back(match);
    }
    return matches;
}

ACMode ACList::check(const IdentityRef& identity, const Permission& permission,
                     const PermissionMatcher& matcher,
                     const ACResolvePolicy& resolver) const {
    return resolver.resolve(find(identity, permission, matcher));
}

ACMode ACList::checkAny(const std::vector<Identity>& identities, const Permission& permission,
                        const PermissionMatcher& matcher,
                        const ACResolvePolicy& resolver) const {
    const auto now = std::chrono::system_clock::now();
    std::vector<ACMatch> all;
    for (const auto& identity : identities) {
        if (!identity.isActive(now)) {
            continue;
        }
        auto matches = find(identity.ref(), permission, matcher);
        for (auto& match : matches) {
            match.identitySpecificity += identitySourcePriority(identity.source);
            all.push_back(match);
        }
    }
    return resolver.resolve(all);
}

void populateDemoAcl(ACList& acl) {
    const Realm& global = Realm::GLOBAL;
    acl.clear();
    acl.add(ACEntry{IdentityRef{"role", global, "operator"},
                    ACRule{"fab.order.view", ACMode::Allow}});
    acl.add(ACEntry{IdentityRef{"role", global, "operator"},
                    ACRule{"fab.order.modify", ACMode::Allow}});
    acl.add(ACEntry{IdentityRef{"user", global, "bob"},
                    ACRule{"fab.order.delete", ACMode::Deny}});
    acl.add(ACEntry{IdentityRef{"anonymous", global, "default"},
                    ACRule{"fab.order.view", ACMode::Allow}});
    acl.add(ACEntry{IdentityRef{"role", global, "operator"}, ACRule{"file.save", ACMode::Allow}});
    acl.add(ACEntry{IdentityRef{"role", global, "operator"}, ACRule{"file.*", ACMode::Allow}});
}

FileACList::FileACList(VolumeFile file, VariantPtr<ACList> wrapped)
    : DecoratedACList(std::move(wrapped)), m_file(std::move(file)) {
    loadFromDisk();
}

FileACList::FileACList(std::filesystem::path path, VariantPtr<ACList> wrapped)
    : DecoratedACList(std::move(wrapped)),
      m_ownedVolume(std::make_unique<LocalVolume>(
          path.parent_path().empty() ? "." : path.parent_path().string())),
      m_file(m_ownedVolume.get(), path.filename().string()) {
    loadFromDisk();
}

void FileACList::loadFromDisk() {
    m_loadedCount = 0;
    wrapped()->clear();
    if (!m_file.exists() || !m_file.isFile()) {
        return;
    }

    const auto content = m_file.readFileUTF8Opt(std::nullopt);
    if (!content.has_value() || content->empty()) {
        return;
    }

    boost::json::value root;
    try {
        root = boost::json::parse(*content);
    } catch (const std::exception&) {
        return;
    }
    if (!root.is_object()) {
        return;
    }

    ACListJsonForm form(*wrapped());
    form.jsonIn(root.as_object(), JsonFormOptions::DEFAULT);
    m_loadedCount = wrapped()->entries().size();
}

void FileACList::saveToDisk() {
    m_file.createParentDirectories();
    boost::json::object envelope;
    ACListJsonForm form(*wrapped());
    form.jsonOut(envelope, JsonFormOptions::DEFAULT);
    m_file.writeFileString(boost::json::serialize(envelope), "UTF-8");
}

void FileACList::flush() { saveToDisk(); }

void FileACList::reload() { loadFromDisk(); }

void FileACList::add(const ACEntry& entry) {
    DecoratedACList::add(entry);
    saveToDisk();
}

void FileACList::remove(const ACEntry& entry) {
    DecoratedACList::remove(entry);
    saveToDisk();
}

void FileACList::clear() {
    DecoratedACList::clear();
    m_loadedCount = 0;
    saveToDisk();
}

} // namespace bas::security
