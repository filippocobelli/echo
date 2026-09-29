#pragma once
#include <Print.h>
#include <Stream.h>
#include <expat.h>

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * Upper bound on entries kept in RAM for a single folder listing.
 * A PROPFIND with Depth:1 on a large cloud folder can return hundreds of
 * <response> elements; anything past this cap is dropped and the caller is
 * told the listing was truncated.
 */
constexpr size_t MAX_WEBDAV_ENTRIES = 64;

/**
 * One member of a WebDAV collection, as reported by a PROPFIND response.
 */
struct WebDavEntry {
  bool isCollection = false;
  std::string name;   // Display name, percent-decoded, for the UI
  std::string path;   // Absolute server path, still percent-encoded, for the next request
  uint32_t size = 0;  // getcontentlength, 0 for collections or when absent
};

/**
 * Path helpers shared by the parser and the WebDAV client.
 * WebDAV hrefs are percent-encoded absolute paths (sometimes full URLs), so
 * paths travel through the app encoded and are only decoded for display.
 */
namespace WebDavPath {
// Percent-decode. '+' is left alone: this is a path, not a query string.
std::string decode(const std::string& value);

// Percent-encode anything unsafe in a path, leaving '/' and already-valid
// "%XX" escapes untouched, so encoding an encoded path is a no-op.
std::string encode(const std::string& path);

// "https://host/dav/x" -> "/dav/x". Paths without an origin are returned as-is.
std::string stripOrigin(const std::string& href);

// Ensure a leading '/', collapse repeated '/', drop the trailing '/' (except root).
std::string normalize(const std::string& path);

// Last path segment, still encoded ("" for the root).
std::string lastSegment(const std::string& path);

// Append a child segment to a normalized parent path.
std::string join(const std::string& base, const std::string& child);

// Parent of a normalized path ("/" for the root).
std::string parent(const std::string& path);
}  // namespace WebDavPath

enum class WebDavParserError { NONE, NO_ENTRY_BUFFER, INVALID_INPUT, PARSER_MEMORY, BUFFER_MEMORY, XML_PARSE };

/**
 * Streaming parser for a WebDAV PROPFIND multistatus response.
 *
 * Deliberately minimal: it only pulls href, resourcetype/collection,
 * displayname and getcontentlength out of each <response>, which is everything
 * a file browser needs. Element names are matched ignoring both the XML
 * namespace prefix (servers use d:, D:, lp1: ...) and case.
 *
 * Written as a Print sink so an HTTP body can be streamed through it without
 * ever buffering the whole response — see WebDavParserStream.
 */
class WebDavParser final : public Print {
 public:
  WebDavParser(WebDavEntry* entries, size_t entryCapacity);
  ~WebDavParser();

  WebDavParser(const WebDavParser&) = delete;
  WebDavParser& operator=(const WebDavParser&) = delete;

  size_t write(uint8_t) override;
  size_t write(const uint8_t*, size_t) override;
  void flush() override;

  bool parse(const char* xmlData, size_t length);

  bool error() const { return errorOccured; }
  WebDavParserError getErrorReason() const { return errorReason; }
  explicit operator bool() const { return !error(); }

  size_t getEntryCount() const { return entryCount; }
  bool wasTruncated() const { return truncated; }

  void clear();

 private:
  static void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char** atts);
  static void XMLCALL endElement(void* userData, const XML_Char* name);
  static void XMLCALL characterData(void* userData, const XML_Char* s, int len);
  bool resetXmlParser();
  void finishEntry();

  XML_Parser parser = nullptr;
  WebDavEntry* entries = nullptr;
  size_t entryCapacity = 0;
  size_t entryCount = 0;

  WebDavEntry currentEntry;
  std::string currentText;
  std::string currentHref;

  bool inResponse = false;
  bool inHref = false;
  bool inResourceType = false;
  bool inDisplayName = false;
  bool inContentLength = false;

  bool errorOccured = false;
  WebDavParserError errorReason = WebDavParserError::NONE;
  bool truncated = false;
};

/**
 * Stream adapter so HTTPClient::writeToStream() can feed the parser directly.
 * Mirrors OpdsParserStream: the destructor finalises the parse.
 */
class WebDavParserStream final : public Stream {
 public:
  explicit WebDavParserStream(WebDavParser& parser) : parser(parser) {}
  ~WebDavParserStream() override { parser.flush(); }

  int available() override { return 0; }
  int peek() override { return -1; }
  int read() override { return -1; }

  size_t write(uint8_t c) override { return parser.write(c); }
  size_t write(const uint8_t* buffer, size_t size) override { return parser.write(buffer, size); }

 private:
  WebDavParser& parser;
};
