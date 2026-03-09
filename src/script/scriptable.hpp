#ifndef SCRIPTABLE_H
#define SCRIPTABLE_H

#include <memory>
#include <string>
#include <vector>

typedef union {
    bool bool_value;
    int int_value;
    double double_value;
    const char *string_value;
    void *pointer_value;
} variant_t;

/** Scriptable proxy: one element on the UI stack (e.g. MainWindow, PDFViewer). */
class IScriptSymbols {
public:
    virtual std::string className() const {
        return typeid(*this).name(); 
    }
    
    virtual std::vector<std::string> findVars(std::string_view prefix) = 0;

    virtual bool hasVar(std::string_view name) = 0;

    virtual std::string formatVar(std::string_view name) = 0;

    /** return false if name is undefined. */
    virtual void parseVar(std::string_view name, std::string_view str) = 0;

    virtual std::vector<std::string> findMethods(std::string_view prefix) = 0;

    virtual bool hasMethod(std::string_view name) = 0;

    virtual int invokeMethod(std::string_view name, const char* const* argv, int argc) = 0;

};

class ScriptApp : public IScriptSymbols {
public:
    std::string className() const override { return m_name; }

    std::vector<std::string> findVars(std::string_view prefix) override;

    bool hasVar(std::string_view name) override;

    std::string formatVar(std::string_view name) override;

    /** return false if name is undefined. */
    void parseVar(std::string_view name, std::string_view str) override;

    std::vector<std::string> findMethods(std::string_view prefix) override;

    bool hasMethod(std::string_view name) override;
    
    int invokeMethod(std::string_view name, const char* const* argv, int argc) override;

private:
    std::string m_name;
    std::vector<std::unique_ptr<IScriptSymbols>> m_stack;
};

extern ScriptApp script_app;

#endif /* SCRIPTABLE_H */
