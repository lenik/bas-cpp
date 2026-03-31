#ifndef HTMLFORM_H
#define HTMLFORM_H

#include "../net/URL.hpp"

class IHtmlForm {
  public:
    /**
     * @param html HTML string to parse.
     * @param url Optional URL to use for relative links.
     * @param file Optional file to use for relative links.
     */
    virtual void htmlIn(std::string_view html
        , const URL* url = nullptr
        , const std::string* file = nullptr) = 0;

    /** 
     * @param url Optional URL to use for relative links.
     * @param file Optional file to use for relative links.
     * @return HTML string.
     */
    virtual std::string htmlOut(const URL* url = nullptr
        , const std::string* file = nullptr) = 0;
};

/**
 * Convert a file to HTML.
 *
 * @param file Input file to convert.
 * @param format Format to convert to. Default determined by the file extension.
 * @param output Output file to write to. empty string means output to stdout.
 */
int pandoc_html(std::string file, std::string format = "", std::string output = "");

#endif // HTMLFORM_H