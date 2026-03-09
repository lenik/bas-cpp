#include "HtmlForm.hpp"

#include "../util/FileTypeDetector.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include <sys/wait.h>

#include <unistd.h>

std::string mimeTypeToPandocFormat(std::string mimeType) {
    if (mimeType == "application/gzip") return "gz";
    if (mimeType == "application/msaccess") return "accdb";
    if (mimeType == "application/msword") return "doc";
    if (mimeType == "application/octet-stream") return "bin";
    if (mimeType == "application/pdf") return "pdf";
    if (mimeType == "application/vnd.ms-powerpoint") return "ppt";
    if (mimeType == "application/vnd.oasis.opendocument.presentation") return "odp";
    if (mimeType == "application/vnd.oasis.opendocument.text") return "odt";
    if (mimeType == "application/vnd.openxmlformats-officedocument.presentationml.presentation") return "pptx";
    if (mimeType == "application/vnd.openxmlformats-officedocument.wordprocessingml.document") return "docx";
    if (mimeType == "application/vnd.rar") return "rar";
    if (mimeType == "application/x-7z-compressed") return "7z";
    if (mimeType == "application/x-apple-diskimage") return "dmg";
    if (mimeType == "application/x-bzip2") return "bz2";
    if (mimeType == "application/x-compressed-iso") return "cso";
    if (mimeType == "application/x-dbf") return "dbf";
    if (mimeType == "application/x-firebird") return "fdb";
    if (mimeType == "application/x-gdb") return "gdb";
    if (mimeType == "application/x-iso9660-image") return "iso";
    if (mimeType == "application/x-msaccess") return "mdb";
    if (mimeType == "application/x-qemu-disk") return "qcow2";
    if (mimeType == "application/x-sqlite3") return "sqlite";
    if (mimeType == "application/x-tar") return "tar";
    if (mimeType == "application/x-virtualbox-vdi") return "vdi";
    if (mimeType == "application/x-virtualbox-vhd") return "vhd";
    if (mimeType == "application/x-virtualbox-vmdk") return "vmdk";
    if (mimeType == "application/x-xz") return "xz";
    if (mimeType == "application/zip") return "zip";
    if (mimeType == "image/bmp") return "bmp";
    if (mimeType == "image/gif") return "gif";
    if (mimeType == "image/ico") return "ico";
    if (mimeType == "image/jpeg") return "jpeg";
    if (mimeType == "image/png") return "png";
    if (mimeType == "image/svg+xml") return "svg";
    if (mimeType == "image/tiff") return "tiff";
    if (mimeType == "image/webp") return "webp";
    if (mimeType == "text/csv") return "csv";
    if (mimeType == "text/html") return "html";
    if (mimeType == "text/markdown") return "markdown";
    if (mimeType == "text/plain") return "plain";
    if (mimeType == "text/plain") return "txt";
    if (mimeType == "text/tab-separated-values") return "tsv";
    if (mimeType == "text/x-c++hdr") return "h";
    if (mimeType == "text/x-c") return "c";
    if (mimeType == "text/x-c++") return "cpp";
    if (mimeType == "text/x-go") return "go";
    if (mimeType == "text/x-iwork-numbers-sffnumbers") return "numbers";
    if (mimeType == "text/x-java") return "java";
    if (mimeType == "text/x-jsx") return "jsx";
    if (mimeType == "text/x-php") return "php";
    if (mimeType == "text/x-python") return "py";
    if (mimeType == "text/x-ruby") return "rb";
    if (mimeType == "text/x-rust") return "rs";
    if (mimeType == "text/x-shellscript") return "sh";
    if (mimeType == "text/x-tsx") return "tsx";
    if (mimeType == "text/x-typescript") return "ts";
    if (mimeType == "text/yaml") return "yaml";
    return "unknown";
}

int pandoc_html(std::string file, std::string format, std::string output) {
    std::vector<std::string> args;
    args.push_back("pandoc");

    if (format.empty()) {
        auto type = FileTypeDetector::getMimeType(file);
        // convert type to pandoc format
        format = mimeTypeToPandocFormat(type);
    }
    args.push_back("-f");
    args.push_back(format);
    
    args.push_back("-t");
    args.push_back("html");
    
    args.push_back("-o");
    args.push_back(output);
    
    args.push_back(file);

    char** argv = new char*[args.size() + 1];
    for (size_t i = 0; i < args.size(); i++) {
        argv[i] = const_cast<char*>(args[i].c_str());
    }
    argv[args.size()] = nullptr;

    pid_t pid = fork(); 
    if (pid == 0) {
        execv("/usr/bin/pandoc", argv);
        exit(1);
    }
    int status = waitpid(pid, nullptr, 0);
    delete[] argv;
    return status;
}
