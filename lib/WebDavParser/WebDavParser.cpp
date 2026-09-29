#include "WebDavParser.h"

#include <Logging.h>
#include <XmlParserUtils.h>
#include <strings.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
// A single href or displayname should never be this long; the cap keeps a
// malformed or hostile response from growing the heap without bound.
constexpr size_t MAX_TEXT_LENGTH = 512;
constexpr size_t PARSE_CHUNK_SIZE = 1024;

bool isHexDigit(const char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; }

int hexValue(const char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Characters that are legal unescaped inside a URL path segment (RFC 3986
// unreserved + sub-delims + ':' '@'). Everything else gets percent-encoded.
bool isPathSafe(const unsigned char c) {
  if (std::isalnum(c)) return true;
  switch (c) {
    case '-':
    case '_':
    case '.':
    case '~':
    case '!':
    case '$':
    case '&':
    case '\'':
    case '(':
    case ')':
    case '*':
    case '+':
    case ',':
    case ';':
    case '=':
    case ':':
    case '@':
      return true;
    default:
      return false;
  }
}

// Element names arrive with their namespace prefix attached ("d:href",
// "lp1:getcontentlength"). Compare only the local part, ignoring case.
bool localNameIs(const char* name, const char* expected) {
  if (!name) return false;
  const char* colon = std::strrchr(name, ':');
  const char* local = colon ? colon + 1 : name;
  return strcasecmp(local, expected) == 0;
}

void appendText(std::string& target, const char* data, const int length) {
  if (length <= 0 || target.size() >= MAX_TEXT_LENGTH) return;
  const size_t room = MAX_TEXT_LENGTH - target.size();
  target.append(data, std::min(room, static_cast<size_t>(length)));
}

std::string trimmed(const std::string& value) {
  size_t start = 0;
  size_t end = value.size();
  while (start < end && std::isspace(static_cast<unsigned char>(value[start]))) start++;
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) end--;
  return value.substr(start, end - start);
}
}  // namespace

// ---------------------------------------------------------------------------
// WebDavPath
// ---------------------------------------------------------------------------

std::string WebDavPath::decode(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (size_t i = 0; i < value.size(); i++) {
    if (value[i] == '%' && i + 2 < value.size() && isHexDigit(value[i + 1]) && isHexDigit(value[i + 2])) {
      out += static_cast<char>((hexValue(value[i + 1]) << 4) | hexValue(value[i + 2]));
      i += 2;
      continue;
    }
    out += value[i];
  }
  return out;
}

std::string WebDavPath::encode(const std::string& path) {
  std::string out;
  out.reserve(path.size() + 8);
  for (size_t i = 0; i < path.size(); i++) {
    const unsigned char c = static_cast<unsigned char>(path[i]);
    if (c == '/' || isPathSafe(c)) {
      out += static_cast<char>(c);
      continue;
    }
    // Leave an existing "%XX" escape alone so encoding stays idempotent.
    if (c == '%' && i + 2 < path.size() && isHexDigit(path[i + 1]) && isHexDigit(path[i + 2])) {
      out.append(path, i, 3);
      i += 2;
      continue;
    }
    char buffer[4];
    snprintf(buffer, sizeof(buffer), "%%%02X", c);
    out += buffer;
  }
  return out;
}

std::string WebDavPath::stripOrigin(const std::string& href) {
  const size_t protocolEnd = href.find("://");
  if (protocolEnd == std::string::npos) return href;
  const size_t pathStart = href.find('/', protocolEnd + 3);
  return pathStart == std::string::npos ? std::string("/") : href.substr(pathStart);
}

std::string WebDavPath::normalize(const std::string& path) {
  std::string out;
  out.reserve(path.size() + 1);
  out += '/';
  for (const char c : path) {
    if (c == '/' && !out.empty() && out.back() == '/') continue;
    out += c;
  }
  if (out.size() > 1 && out.back() == '/') out.pop_back();
  return out;
}

std::string WebDavPath::lastSegment(const std::string& path) {
  const std::string normalized = normalize(path);
  const size_t slash = normalized.find_last_of('/');
  return slash == std::string::npos ? normalized : normalized.substr(slash + 1);
}

std::string WebDavPath::join(const std::string& base, const std::string& child) {
  const std::string normalizedBase = normalize(base);
  size_t childStart = 0;
  while (childStart < child.size() && child[childStart] == '/') childStart++;
  if (childStart >= child.size()) return normalizedBase;
  return normalize(normalizedBase + "/" + child.substr(childStart));
}

std::string WebDavPath::parent(const std::string& path) {
  const std::string normalized = normalize(path);
  const size_t slash = normalized.find_last_of('/');
  if (slash == std::string::npos || slash == 0) return "/";
  return normalized.substr(0, slash);
}

// ---------------------------------------------------------------------------
// WebDavParser
// ---------------------------------------------------------------------------

WebDavParser::WebDavParser(WebDavEntry* entries, const size_t entryCapacity)
    : entries(entries), entryCapacity(entryCapacity) {
  if (!entries || entryCapacity == 0) {
    errorOccured = true;
    errorReason = WebDavParserError::NO_ENTRY_BUFFER;
    LOG_DBG("WEBDAV", "No entry buffer supplied");
  }
  resetXmlParser();
}

WebDavParser::~WebDavParser() { destroyXmlParser(parser); }

size_t WebDavParser::write(const uint8_t c) { return write(&c, 1); }

size_t WebDavParser::write(const uint8_t* xmlData, const size_t length) {
  if (errorOccured || !parser) {
    errorOccured = true;
    return length;
  }
  if (!xmlData && length > 0) {
    errorOccured = true;
    errorReason = WebDavParserError::INVALID_INPUT;
    return length;
  }

  const char* currentPos = reinterpret_cast<const char*>(xmlData);
  size_t remaining = length;

  while (remaining > 0) {
    const size_t toRead = remaining < PARSE_CHUNK_SIZE ? remaining : PARSE_CHUNK_SIZE;
    void* const buffer = XML_GetBuffer(parser, static_cast<int>(toRead));
    if (!buffer) {
      errorOccured = true;
      errorReason = WebDavParserError::BUFFER_MEMORY;
      LOG_ERR("WEBDAV", "Out of memory for XML buffer");
      destroyXmlParser(parser);
      return length;
    }

    memcpy(buffer, currentPos, toRead);

    if (XML_ParseBuffer(parser, static_cast<int>(toRead), 0) == XML_STATUS_ERROR) {
      errorOccured = true;
      errorReason = WebDavParserError::XML_PARSE;
      LOG_ERR("WEBDAV", "Parse error at line %lu: %s", XML_GetCurrentLineNumber(parser),
              XML_ErrorString(XML_GetErrorCode(parser)));
      destroyXmlParser(parser);
      return length;
    }
    currentPos += toRead;
    remaining -= toRead;
  }
  return length;
}

void WebDavParser::flush() {
  if (!parser) return;
  if (XML_Parse(parser, nullptr, 0, XML_TRUE) != XML_STATUS_OK) {
    errorOccured = true;
    errorReason = WebDavParserError::XML_PARSE;
    destroyXmlParser(parser);
  }
}

bool WebDavParser::parse(const char* xmlData, const size_t length) {
  clear();
  if (!xmlData && length > 0) {
    errorOccured = true;
    errorReason = WebDavParserError::INVALID_INPUT;
    return false;
  }
  if (length > 0) {
    write(reinterpret_cast<const uint8_t*>(xmlData), length);
  }
  flush();
  return !error();
}

void WebDavParser::clear() {
  entryCount = 0;
  truncated = false;
  currentEntry = WebDavEntry{};
  currentText.clear();
  currentHref.clear();
  inResponse = inHref = inResourceType = inDisplayName = inContentLength = false;
  errorOccured = !entries || entryCapacity == 0;
  errorReason = errorOccured ? WebDavParserError::NO_ENTRY_BUFFER : WebDavParserError::NONE;
  resetXmlParser();
}

bool WebDavParser::resetXmlParser() {
  if (parser && XML_ParserReset(parser, nullptr) != XML_TRUE) {
    destroyXmlParser(parser);
  }

  if (!parser) {
    parser = XML_ParserCreate(nullptr);
    if (!parser) {
      errorOccured = true;
      errorReason = WebDavParserError::PARSER_MEMORY;
      LOG_ERR("WEBDAV", "Failed to create XML parser");
      return false;
    }
  }

  XML_SetUserData(parser, this);
  XML_SetElementHandler(parser, startElement, endElement);
  XML_SetCharacterDataHandler(parser, characterData);
  return true;
}

void XMLCALL WebDavParser::startElement(void* userData, const XML_Char* name, const XML_Char** atts) {
  (void)atts;
  auto* self = static_cast<WebDavParser*>(userData);

  if (localNameIs(name, "response")) {
    self->inResponse = true;
    self->currentEntry = WebDavEntry{};
    self->currentHref.clear();
    return;
  }
  if (!self->inResponse) return;

  if (localNameIs(name, "href")) {
    // Only the first href in a response identifies the member; later ones
    // (inside <lockdiscovery> for instance) must not overwrite it.
    if (self->currentHref.empty()) {
      self->inHref = true;
      self->currentText.clear();
    }
  } else if (localNameIs(name, "resourcetype")) {
    self->inResourceType = true;
  } else if (localNameIs(name, "collection")) {
    if (self->inResourceType) self->currentEntry.isCollection = true;
  } else if (localNameIs(name, "displayname")) {
    self->inDisplayName = true;
    self->currentText.clear();
  } else if (localNameIs(name, "getcontentlength")) {
    self->inContentLength = true;
    self->currentText.clear();
  }
}

void XMLCALL WebDavParser::endElement(void* userData, const XML_Char* name) {
  auto* self = static_cast<WebDavParser*>(userData);

  if (localNameIs(name, "response")) {
    self->finishEntry();
    self->inResponse = false;
    return;
  }
  if (!self->inResponse) return;

  if (localNameIs(name, "href")) {
    if (self->inHref) {
      self->currentHref = trimmed(self->currentText);
      self->inHref = false;
    }
  } else if (localNameIs(name, "resourcetype")) {
    self->inResourceType = false;
  } else if (localNameIs(name, "displayname")) {
    if (self->inDisplayName) {
      // A failed (404) propstat repeats the property as an empty element, so
      // never let an empty value clobber one we already captured.
      const std::string value = trimmed(self->currentText);
      if (!value.empty()) self->currentEntry.name = value;
      self->inDisplayName = false;
    }
  } else if (localNameIs(name, "getcontentlength")) {
    if (self->inContentLength) {
      const std::string value = trimmed(self->currentText);
      if (!value.empty()) self->currentEntry.size = static_cast<uint32_t>(strtoul(value.c_str(), nullptr, 10));
      self->inContentLength = false;
    }
  }
  self->currentText.clear();
}

void XMLCALL WebDavParser::characterData(void* userData, const XML_Char* s, const int len) {
  auto* self = static_cast<WebDavParser*>(userData);
  if (self->inHref || self->inDisplayName || self->inContentLength) {
    appendText(self->currentText, s, len);
  }
}

void WebDavParser::finishEntry() {
  if (currentHref.empty()) return;

  if (entryCount >= entryCapacity) {
    truncated = true;
    return;
  }

  currentEntry.path = WebDavPath::normalize(WebDavPath::stripOrigin(currentHref));
  if (currentEntry.name.empty()) {
    currentEntry.name = WebDavPath::decode(WebDavPath::lastSegment(currentEntry.path));
  }
  if (currentEntry.isCollection) currentEntry.size = 0;

  entries[entryCount++] = std::move(currentEntry);
  currentEntry = WebDavEntry{};
  currentHref.clear();
}
