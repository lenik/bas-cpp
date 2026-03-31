#include "FileTypeDetector.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

static void test_detect_by_extension() {
    assert(FileTypeDetector::detectFileTypeByExtension("txt") == FileTypeDetector::TEXT);
    assert(FileTypeDetector::detectFileTypeByExtension("TXT") == FileTypeDetector::TEXT);
    assert(FileTypeDetector::detectFileTypeByExtension("pdf") == FileTypeDetector::DOCUMENT);
    assert(FileTypeDetector::detectFileTypeByExtension("png") == FileTypeDetector::IMAGE);
    assert(FileTypeDetector::detectFileTypeByExtension("jpg") == FileTypeDetector::IMAGE);
    assert(FileTypeDetector::detectFileTypeByExtension("mp3") == FileTypeDetector::AUDIO);
    assert(FileTypeDetector::detectFileTypeByExtension("mp4") == FileTypeDetector::VIDEO);
    assert(FileTypeDetector::detectFileTypeByExtension("zip") == FileTypeDetector::ARCHIVE);
    assert(FileTypeDetector::detectFileTypeByExtension("xlsx") == FileTypeDetector::SPREADSHEET);
    assert(FileTypeDetector::detectFileTypeByExtension("sqlite") == FileTypeDetector::DATABASE);
    assert(FileTypeDetector::detectFileTypeByExtension("iso") == FileTypeDetector::FS_IMAGE);
    assert(FileTypeDetector::detectFileTypeByExtension("exe") == FileTypeDetector::EXECUTABLE);
    assert(FileTypeDetector::detectFileTypeByExtension("") == FileTypeDetector::DIRECTORY);
    assert(FileTypeDetector::detectFileTypeByExtension("xyz") == FileTypeDetector::UNKNOWN);
}

static void test_detect_by_filename() {
    assert(FileTypeDetector::detectFileType("doc.txt") == FileTypeDetector::TEXT);
    assert(FileTypeDetector::detectFileType("/path/to/file.PDF") == FileTypeDetector::DOCUMENT);
    assert(FileTypeDetector::detectFileType("image.png") == FileTypeDetector::IMAGE);
    assert(FileTypeDetector::detectFileType("archive.tar.gz") == FileTypeDetector::ARCHIVE);
    assert(FileTypeDetector::detectFileType("noext") == FileTypeDetector::DIRECTORY);  // empty ext -> DIRECTORY
    assert(FileTypeDetector::detectFileType("file.xyz") == FileTypeDetector::UNKNOWN);
}

static void test_detect_by_content() {
    std::vector<uint8_t> pdf = {'%', 'P', 'D', 'F'};
    assert(FileTypeDetector::detectFileTypeByContent(pdf) == FileTypeDetector::DOCUMENT);

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    assert(FileTypeDetector::detectFileTypeByContent(png) == FileTypeDetector::IMAGE);

    const unsigned char jpeg_sig[] = {0xff, 0xd8, 0xff, 0xe0};
    std::vector<uint8_t> jpeg(jpeg_sig, jpeg_sig + 4);
    assert(FileTypeDetector::detectFileTypeByContent(jpeg) == FileTypeDetector::IMAGE);

    std::vector<uint8_t> zip = {'P', 'K', 0x03, 0x04};
    assert(FileTypeDetector::detectFileTypeByContent(zip) == FileTypeDetector::ARCHIVE);

    std::vector<uint8_t> sqlite = {};
    const char* sig = "SQLite format 3";
    sqlite.assign(sig, sig + 16);
    assert(FileTypeDetector::detectFileTypeByContent(sqlite) == FileTypeDetector::DATABASE);

    std::vector<uint8_t> elf = {0x7f, 'E', 'L', 'F'};
    assert(FileTypeDetector::detectFileTypeByContent(elf) == FileTypeDetector::EXECUTABLE);

    std::vector<uint8_t> small = {1, 2, 3};
    assert(FileTypeDetector::detectFileTypeByContent(small) == FileTypeDetector::UNKNOWN);
}

static void test_mime_type() {
    assert(FileTypeDetector::getMimeTypeByExtension("txt") == "text/plain");
    assert(FileTypeDetector::getMimeTypeByExtension("html") == "text/html");
    assert(FileTypeDetector::getMimeTypeByExtension("pdf") == "application/pdf");
    assert(FileTypeDetector::getMimeTypeByExtension("png") == "image/png");
    assert(FileTypeDetector::getMimeType("file.json") == "application/json");
    assert(FileTypeDetector::getMimeType("x.y.z.unknown") == "application/octet-stream");
}

static void test_category_checks() {
    assert(FileTypeDetector::isTextFile("a.txt"));
    assert(FileTypeDetector::isImageFile("x.png"));
    assert(FileTypeDetector::isArchiveFile("a.zip"));
    assert(FileTypeDetector::isDocumentFile("b.pdf"));
    assert(!FileTypeDetector::isTextFile("x.png"));
    assert(!FileTypeDetector::isImageFile("a.txt"));
}

static void test_text_content() {
    std::vector<uint8_t> text = {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '\n'};
    assert(FileTypeDetector::isTextContent(text));
    std::vector<uint8_t> binary = {0x00, 0xff, 0x80, 0x00};
    assert(FileTypeDetector::isBinaryContent(binary));
    assert(!FileTypeDetector::isTextContent(binary));
}

static void test_file_type_string() {
    assert(FileTypeDetector::getFileTypeString(FileTypeDetector::TEXT) == "Text file");
    assert(FileTypeDetector::getFileTypeString(FileTypeDetector::IMAGE) == "Image");
    assert(FileTypeDetector::getFileTypeString(FileTypeDetector::UNKNOWN) == "Unknown");
    assert(FileTypeDetector::getFileDescription("readme.md") == "Text file");
}

int main() {
    test_detect_by_extension();
    test_detect_by_filename();
    test_detect_by_content();
    test_mime_type();
    test_category_checks();
    test_text_content();
    test_file_type_string();
    std::cout << "All FileTypeDetector tests passed.\n";
    return 0;
}
