#include "scriptable.hpp"

#include <unordered_set>

ScriptApp script_app;

std::vector<std::string> ScriptApp::findVars(std::string_view prefix) {
    std::vector<std::string> found;
    std::unordered_set<std::string> used;
    for (auto& scope : m_stack) {
        for (auto& name : scope->findVars(prefix)) {
            if (!used.insert(name).second) continue;
            found.push_back(name);
        }
    }
    return found;
}

bool ScriptApp::hasVar(std::string_view name) {
    for (auto& scope : m_stack) {
        if (scope->hasVar(name)) {
            return true;
        }
    }
    return false;
}

std::string ScriptApp::formatVar(std::string_view name) {
    for (auto& scope : m_stack) {
        if (scope->hasVar(name)) {
            return scope->formatVar(name);
        }
    }
    return "undefined";
}

void ScriptApp::parseVar(std::string_view name, std::string_view str) {
    for (auto& scope : m_stack) {
        if (scope->hasVar(name)) {
            scope->parseVar(name, str);
            return;
        }
    }
}

std::vector<std::string> ScriptApp::findMethods(std::string_view prefix) {
    std::vector<std::string> found;
    std::unordered_set<std::string> used;
    for (auto& scope : m_stack) {
        for (auto& name : scope->findMethods(prefix)) {
            if (!used.insert(name).second) continue;
            found.push_back(name);
        }
    }
    return found;
}

bool ScriptApp::hasMethod(std::string_view name) {
    for (auto& scope : m_stack) {
        if (scope->hasMethod(name)) {
            return true;
        }
    }
    return false;
}

int ScriptApp::invokeMethod(std::string_view name, const char* const* argv, int argc) {
    for (auto& scope : m_stack) {
        if (scope->hasMethod(name)) {
            return scope->invokeMethod(name, argv, argc);
        }
    }
    return -1;
}
