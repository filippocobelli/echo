#include <cstdio>
#include <cstring>
#include <string>

#include "lib/WebDavParser/WebDavParser.h"

static int testsPassed = 0;
static int testsFailed = 0;

#define ASSERT_EQ(a, b)                                                           \
  do {                                                                            \
    auto _a = (a);                                                                \
    auto _b = (b);                                                                \
    if (_a != _b) {                                                               \
      fprintf(stderr, "  FAIL: %s:%d: %s != expected\n", __FILE__, __LINE__, #a); \
      testsFailed++;                                                              \
      return;                                                                     \
    }                                                                             \
  } while (0)

#define ASSERT_STREQ(a, b)                                                                              \
  do {                                                                                                  \
    const std::string _a = (a);                                                                         \
    const std::string _b = (b);                                                                         \
    if (_a != _b) {                                                                                     \
      fprintf(stderr, "  FAIL: %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, _a.c_str(), _b.c_str()); \
      testsFailed++;                                                                                    \
      return;                                                                                           \
    }                                                                                                   \
  } while (0)

#define ASSERT_TRUE(cond)                                                \
  do {                                                                   \
    if (!(cond)) {                                                       \
      fprintf(stderr, "  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      testsFailed++;                                                     \
      return;                                                            \
    }                                                                    \
  } while (0)

#define RUN_TEST(fn)                          \
  do {                                        \
    printf("- %s\n", #fn);                    \
    const int before = testsFailed;           \
    fn();                                     \
    if (testsFailed == before) testsPassed++; \
  } while (0)

// A Koofr-shaped Depth:1 listing: the collection itself first, then members.
// Deliberately mixes namespace prefixes and includes a 404 propstat, which is
// what real servers send for properties a member does not have.
static const char* KOOFR_LISTING = R"XML(<?xml version="1.0" encoding="utf-8"?>
<d:multistatus xmlns:d="DAV:">
  <d:response>
    <d:href>/dav/Koofr/Books/</d:href>
    <d:propstat>
      <d:prop>
        <d:resourcetype><d:collection/></d:resourcetype>
        <d:displayname>Books</d:displayname>
      </d:prop>
      <d:status>HTTP/1.1 200 OK</d:status>
    </d:propstat>
    <d:propstat>
      <d:prop><d:getcontentlength/></d:prop>
      <d:status>HTTP/1.1 404 Not Found</d:status>
    </d:propstat>
  </d:response>
  <d:response>
    <d:href>/dav/Koofr/Books/Sci-Fi/</d:href>
    <d:propstat>
      <d:prop>
        <d:resourcetype><d:collection/></d:resourcetype>
        <d:displayname>Sci-Fi</d:displayname>
      </d:prop>
      <d:status>HTTP/1.1 200 OK</d:status>
    </d:propstat>
  </d:response>
  <D:response xmlns:D="DAV:" xmlns:lp1="DAV:">
    <D:href>/dav/Koofr/Books/Dune%20-%20Frank%20Herbert.epub</D:href>
    <D:propstat>
      <D:prop>
        <D:resourcetype/>
        <lp1:getcontentlength>1048576</lp1:getcontentlength>
        <D:displayname>Dune - Frank Herbert.epub</D:displayname>
      </D:prop>
      <D:status>HTTP/1.1 200 OK</D:status>
    </D:propstat>
  </D:response>
  <d:response>
    <d:href>https://app.koofr.net/dav/Koofr/Books/notes.txt</d:href>
    <d:propstat>
      <d:prop>
        <d:resourcetype/>
        <d:getcontentlength>42</d:getcontentlength>
      </d:prop>
      <d:status>HTTP/1.1 200 OK</d:status>
    </d:propstat>
  </d:response>
</d:multistatus>)XML";

static void test_parses_collection_and_files() {
  WebDavEntry entries[8];
  WebDavParser parser(entries, 8);
  ASSERT_TRUE(parser.parse(KOOFR_LISTING, strlen(KOOFR_LISTING)));
  ASSERT_EQ(parser.getEntryCount(), static_cast<size_t>(4));

  // The collection itself comes back first; the client filters it out by path.
  ASSERT_TRUE(entries[0].isCollection);
  ASSERT_STREQ(entries[0].path, "/dav/Koofr/Books");
  ASSERT_STREQ(entries[0].name, "Books");
  ASSERT_EQ(entries[0].size, static_cast<uint32_t>(0));

  ASSERT_TRUE(entries[1].isCollection);
  ASSERT_STREQ(entries[1].path, "/dav/Koofr/Books/Sci-Fi");
  ASSERT_STREQ(entries[1].name, "Sci-Fi");

  ASSERT_TRUE(!entries[2].isCollection);
  ASSERT_STREQ(entries[2].path, "/dav/Koofr/Books/Dune%20-%20Frank%20Herbert.epub");
  ASSERT_STREQ(entries[2].name, "Dune - Frank Herbert.epub");
  ASSERT_EQ(entries[2].size, static_cast<uint32_t>(1048576));

  // A full-URL href is reduced to its path.
  ASSERT_TRUE(!entries[3].isCollection);
  ASSERT_STREQ(entries[3].path, "/dav/Koofr/Books/notes.txt");
  ASSERT_EQ(entries[3].size, static_cast<uint32_t>(42));
}

static void test_name_falls_back_to_href_segment() {
  static const char* xml = R"XML(<?xml version="1.0"?>
<multistatus xmlns="DAV:">
  <response>
    <href>/dav/Koofr/Libri/Il%20Nome%20della%20Rosa.epub</href>
    <propstat><prop><resourcetype/></prop><status>HTTP/1.1 200 OK</status></propstat>
  </response>
</multistatus>)XML";

  WebDavEntry entries[4];
  WebDavParser parser(entries, 4);
  ASSERT_TRUE(parser.parse(xml, strlen(xml)));
  ASSERT_EQ(parser.getEntryCount(), static_cast<size_t>(1));
  ASSERT_STREQ(entries[0].name, "Il Nome della Rosa.epub");
}

static void test_empty_displayname_does_not_clobber() {
  // Some servers answer displayname in a 404 propstat as an empty element.
  static const char* xml = R"XML(<?xml version="1.0"?>
<d:multistatus xmlns:d="DAV:">
  <d:response>
    <d:href>/dav/book.epub</d:href>
    <d:propstat><d:prop><d:displayname>Real Title.epub</d:displayname></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>
    <d:propstat><d:prop><d:displayname/></d:prop><d:status>HTTP/1.1 404 Not Found</d:status></d:propstat>
  </d:response>
</d:multistatus>)XML";

  WebDavEntry entries[4];
  WebDavParser parser(entries, 4);
  ASSERT_TRUE(parser.parse(xml, strlen(xml)));
  ASSERT_EQ(parser.getEntryCount(), static_cast<size_t>(1));
  ASSERT_STREQ(entries[0].name, "Real Title.epub");
}

static void test_truncates_at_capacity() {
  std::string xml = "<d:multistatus xmlns:d=\"DAV:\">";
  for (int i = 0; i < 10; i++) {
    xml += "<d:response><d:href>/dav/f" + std::to_string(i) +
           ".epub</d:href><d:propstat><d:prop><d:resourcetype/></d:prop><d:status>HTTP/1.1 200 "
           "OK</d:status></d:propstat></d:response>";
  }
  xml += "</d:multistatus>";

  WebDavEntry entries[3];
  WebDavParser parser(entries, 3);
  ASSERT_TRUE(parser.parse(xml.c_str(), xml.size()));
  ASSERT_EQ(parser.getEntryCount(), static_cast<size_t>(3));
  ASSERT_TRUE(parser.wasTruncated());
  ASSERT_STREQ(entries[2].name, "f2.epub");
}

static void test_streamed_in_small_chunks() {
  // The real transport hands the body over in HTTP-sized pieces, so element
  // text can be split across writes.
  WebDavEntry entries[8];
  WebDavParser parser(entries, 8);
  const size_t total = strlen(KOOFR_LISTING);
  for (size_t offset = 0; offset < total; offset += 7) {
    const size_t chunk = std::min(static_cast<size_t>(7), total - offset);
    parser.write(reinterpret_cast<const uint8_t*>(KOOFR_LISTING + offset), chunk);
  }
  parser.flush();
  ASSERT_TRUE(!parser.error());
  ASSERT_EQ(parser.getEntryCount(), static_cast<size_t>(4));
  ASSERT_STREQ(entries[2].name, "Dune - Frank Herbert.epub");
}

static void test_malformed_xml_reports_error() {
  static const char* xml = "<d:multistatus><d:response><d:href>/a</d:href>";
  WebDavEntry entries[4];
  WebDavParser parser(entries, 4);
  ASSERT_TRUE(!parser.parse(xml, strlen(xml)));
  ASSERT_TRUE(parser.error());
  ASSERT_TRUE(parser.getErrorReason() == WebDavParserError::XML_PARSE);
}

static void test_rejects_missing_entry_buffer() {
  WebDavParser parser(nullptr, 0);
  ASSERT_TRUE(parser.error());
  ASSERT_TRUE(parser.getErrorReason() == WebDavParserError::NO_ENTRY_BUFFER);
}

static void test_path_helpers() {
  ASSERT_STREQ(WebDavPath::normalize("dav//Koofr/Books/"), "/dav/Koofr/Books");
  ASSERT_STREQ(WebDavPath::normalize("/"), "/");
  ASSERT_STREQ(WebDavPath::normalize(""), "/");
  ASSERT_STREQ(WebDavPath::stripOrigin("https://app.koofr.net/dav/Koofr"), "/dav/Koofr");
  ASSERT_STREQ(WebDavPath::stripOrigin("https://app.koofr.net"), "/");
  ASSERT_STREQ(WebDavPath::stripOrigin("/dav/Koofr"), "/dav/Koofr");
  ASSERT_STREQ(WebDavPath::lastSegment("/dav/Koofr/Books/"), "Books");
  ASSERT_STREQ(WebDavPath::lastSegment("/"), "");
  ASSERT_STREQ(WebDavPath::join("/dav/Koofr", "/Books"), "/dav/Koofr/Books");
  ASSERT_STREQ(WebDavPath::join("/dav/Koofr", ""), "/dav/Koofr");
  ASSERT_STREQ(WebDavPath::parent("/dav/Koofr/Books"), "/dav/Koofr");
  ASSERT_STREQ(WebDavPath::parent("/dav"), "/");
  ASSERT_STREQ(WebDavPath::parent("/"), "/");

  ASSERT_STREQ(WebDavPath::decode("Dune%20-%20Herbert.epub"), "Dune - Herbert.epub");
  ASSERT_STREQ(WebDavPath::decode("100%"), "100%");
  ASSERT_STREQ(WebDavPath::encode("/Books/Dune - Herbert.epub"), "/Books/Dune%20-%20Herbert.epub");
  // Encoding an already-encoded path must not double-escape it.
  ASSERT_STREQ(WebDavPath::encode("/Books/Dune%20-%20Herbert.epub"), "/Books/Dune%20-%20Herbert.epub");
  ASSERT_STREQ(WebDavPath::encode("/Libri/Perch\xc3\xa9.epub"), "/Libri/Perch%C3%A9.epub");
}

int main() {
  printf("WebDavParser tests\n");
  RUN_TEST(test_parses_collection_and_files);
  RUN_TEST(test_name_falls_back_to_href_segment);
  RUN_TEST(test_empty_displayname_does_not_clobber);
  RUN_TEST(test_truncates_at_capacity);
  RUN_TEST(test_streamed_in_small_chunks);
  RUN_TEST(test_malformed_xml_reports_error);
  RUN_TEST(test_rejects_missing_entry_buffer);
  RUN_TEST(test_path_helpers);

  printf("\n%d passed, %d failed\n", testsPassed, testsFailed);
  return testsFailed == 0 ? 0 : 1;
}
