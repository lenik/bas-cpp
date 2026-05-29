#ifndef BAS_SECURITY_JSON_HPP
#define BAS_SECURITY_JSON_HPP

#include "ac_list.hpp"
#include "credential.hpp"
#include "identity.hpp"
#include "realm.hpp"
#include "identity_service.hpp"

#include "bas/fmt/JsonForm.hpp"

namespace bas::security {

std::string acModeToString(ACMode mode);
ACMode acModeFromString(std::string_view s);

std::string identityStateToString(IdentityState state);
IdentityState identityStateFromString(std::string_view s);

std::string identitySourceToString(IdentitySource source);
IdentitySource identitySourceFromString(std::string_view s);

void identityRefJsonIn(IdentityRef& ref, const boost::json::object& o);
void identityRefJsonOut(const IdentityRef& ref, boost::json::object& o);

void realmJsonIn(Realm& realm, const boost::json::value& in);
void realmJsonOut(const Realm& realm, boost::json::object& o);

void acRuleJsonIn(ACRule& rule, const boost::json::object& o);
void acRuleJsonOut(const ACRule& rule, boost::json::object& o);

void acEntryJsonIn(ACEntry& entry, const boost::json::object& o);
void acEntryJsonOut(const ACEntry& entry, boost::json::object& o);

class ACListJsonForm : public IJsonForm {
  public:
    explicit ACListJsonForm(ACList& list) : m_list(list) {}

    void jsonIn(const boost::json::object& o, const JsonFormOptions& opts) override;
    void jsonOut(boost::json::object& obj, const JsonFormOptions& opts) override;

  private:
    ACList& m_list;
};

void identityJsonIn(Identity& identity, boost::json::object& o);
void identityJsonOut(const Identity& identity, boost::json::object& o);

void identitySetJsonIn(IdentitySet& set, boost::json::object& o);
void identitySetJsonOut(const IdentitySet& set, boost::json::object& o);

void credentialRefJsonIn(CredentialRef& ref, boost::json::object& o);
void credentialRefJsonOut(const CredentialRef& ref, boost::json::object& o);

void credentialMetaJsonIn(CredentialMeta& meta, boost::json::object& o);
void credentialMetaJsonOut(const CredentialMeta& meta, boost::json::object& o);

void credentialInfoJsonIn(CredentialInfo& info, boost::json::object& o);
void credentialInfoJsonOut(const CredentialInfo& info, boost::json::object& o);

void loginFieldJsonIn(LoginField& field, boost::json::object& o);
void loginFieldJsonOut(const LoginField& field, boost::json::object& o);

void loginActionJsonIn(LoginAction& action, boost::json::object& o);
void loginActionJsonOut(const LoginAction& action, boost::json::object& o);

void loginFormSpecJsonIn(LoginFormSpec& form, boost::json::object& o);
void loginFormSpecJsonOut(const LoginFormSpec& form, boost::json::object& o);

} // namespace bas::security

#endif
