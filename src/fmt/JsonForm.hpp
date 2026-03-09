#ifndef JSONFORM_H
#define JSONFORM_H

#include "TextForm.hpp"

#include <boost/json.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/type_traits/is_array.hpp>

struct JsonFormOptions {
    bool typeInfo;
    bool compact;
    int indent;

    int maxDepth;

    bool includeNull;
    bool includeFalse = true;

    static const JsonFormOptions DEFAULT;
    static const JsonFormOptions COMPACT;
};

class IJsonForm : public ITextForm {
public:
    IJsonForm() = default;
    virtual ~IJsonForm() {}

    void readObject(Reader* in) override {
        std::string text = in->readCharsUntilEOF();
        boost::json::value json = boost::json::parse(text);
        jsonIn(json, JsonFormOptions::DEFAULT);
    }

    void writeObject(Writer* out) override {
        boost::json::value json = jsonOut(JsonFormOptions::DEFAULT);
        std::string text = serialize(json);
        out->write(text);
    }

public:
    virtual bool likeObject() { return true; }

    virtual void jsonIn(boost::json::value &in,
                        const JsonFormOptions &opts = JsonFormOptions::DEFAULT) {
        if (in.is_null()) {
            return;
        }
        jsonIn(in.get_object(), opts);
    }

    virtual void jsonIn(boost::json::object &o, const JsonFormOptions &opts) = 0;

    boost::json::value jsonOut(const JsonFormOptions &opts = JsonFormOptions::DEFAULT) {
        boost::json::object obj;
        jsonOut(obj, opts);
        return obj;
    }

    virtual void jsonOut(boost::json::object &obj, const JsonFormOptions &opts) = 0;
};

#endif // JSONFORM_H