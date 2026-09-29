#include <SimHttpFetch.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "src/network/WebDavClient.h"

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
    resetTransport();                         \
    fn();                                     \
    if (testsFailed == before) testsPassed++; \
  } while (0)

static WebDavClient::Account koofrAccount() {
  return WebDavClient::Account{"https://app.koofr.net/dav/Koofr", "reader@example.com", "app-password"};
}

// Members are deliberately out of order, and the collection itself is included,
// exactly as a Depth:1 reply arrives.
static const char* BOOKS_LISTING = R"XML(<?xml version="1.0" encoding="utf-8"?>
<d:multistatus xmlns:d="DAV:">
  <d:response>
    <d:href>/dav/Koofr/Books/</d:href>
    <d:propstat><d:prop><d:resourcetype><d:collection/></d:resourcetype><d:displayname>Books</d:displayname></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>
  </d:response>
  <d:response>
    <d:href>/dav/Koofr/Books/zeta.epub</d:href>
    <d:propstat><d:prop><d:resourcetype/><d:getcontentlength>2048</d:getcontentlength><d:displayname>zeta.epub</d:displayname></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>
  </d:response>
  <d:response>
    <d:href>/dav/Koofr/Books/Alpha.epub</d:href>
    <d:propstat><d:prop><d:resourcetype/><d:getcontentlength>1024</d:getcontentlength><d:displayname>Alpha.epub</d:displayname></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>
  </d:response>
  <d:response>
    <d:href>/dav/Koofr/Books/Sci-Fi/</d:href>
    <d:propstat><d:prop><d:resourcetype><d:collection/></d:resourcetype><d:displayname>Sci-Fi</d:displayname></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>
  </d:response>
</d:multistatus>)XML";

static void resetTransport() {
  sim_http_fetch::transportAvailable() = true;
  sim_http_fetch::cannedResponse() = sim_http_fetch::Response{207, BOOKS_LISTING};
  sim_http_fetch::lastRequest() = sim_http_fetch::RecordedRequest{};
}

static void test_root_path_combines_url_and_root_folder() {
  const auto account = koofrAccount();
  ASSERT_STREQ(WebDavClient::rootPathFor(account, "/"), "/dav/Koofr");
  ASSERT_STREQ(WebDavClient::rootPathFor(account, ""), "/dav/Koofr");
  ASSERT_STREQ(WebDavClient::rootPathFor(account, "/Books"), "/dav/Koofr/Books");
  ASSERT_STREQ(WebDavClient::rootPathFor(account, "Books/Sci Fi"), "/dav/Koofr/Books/Sci%20Fi");

  // A URL typed without a scheme must still yield the path, not the host.
  const WebDavClient::Account noScheme{"app.koofr.net/dav/Koofr", "", ""};
  ASSERT_STREQ(WebDavClient::rootPathFor(noScheme, "/"), "/dav/Koofr");
  ASSERT_STREQ(WebDavClient::rootPathFor(WebDavClient::Account{}, "/Books"), "/");
}

static void test_url_for_path_keeps_origin() {
  const auto account = koofrAccount();
  ASSERT_STREQ(WebDavClient::urlForPath(account, "/dav/Koofr/Books"), "https://app.koofr.net/dav/Koofr/Books");
  ASSERT_STREQ(WebDavClient::urlForPath(account, "/dav/Koofr/Books/Dune%20I.epub"),
               "https://app.koofr.net/dav/Koofr/Books/Dune%20I.epub");
}

static void test_lists_members_sorted_without_self() {
  WebDavEntry entries[8];
  size_t count = 0;
  bool truncated = true;

  const auto error = WebDavClient::listDirectory(koofrAccount(), "/dav/Koofr/Books", entries, 8, count, truncated);
  ASSERT_TRUE(error == WebDavClient::Error::OK);
  ASSERT_TRUE(!truncated);

  // The collection's own response is dropped; folders sort ahead of files, and
  // files sort case-insensitively.
  ASSERT_EQ(count, static_cast<size_t>(3));
  ASSERT_STREQ(entries[0].name, "Sci-Fi");
  ASSERT_TRUE(entries[0].isCollection);
  ASSERT_STREQ(entries[1].name, "Alpha.epub");
  ASSERT_STREQ(entries[2].name, "zeta.epub");
  ASSERT_EQ(entries[2].size, static_cast<uint32_t>(2048));
}

static void test_request_shape() {
  WebDavEntry entries[8];
  size_t count = 0;
  bool truncated = false;
  WebDavClient::listDirectory(koofrAccount(), "/dav/Koofr/Books", entries, 8, count, truncated);

  const auto& request = sim_http_fetch::lastRequest();
  ASSERT_STREQ(request.method, "PROPFIND");
  ASSERT_STREQ(request.url, "https://app.koofr.net/dav/Koofr/Books");
  ASSERT_STREQ(request.headers.at("Depth"), "1");
  ASSERT_STREQ(request.basicAuth, "reader@example.com:app-password");
  ASSERT_TRUE(request.body.find("<D:resourcetype/>") != std::string::npos);
  ASSERT_TRUE(request.body.find("<D:getcontentlength/>") != std::string::npos);
}

static void test_anonymous_account_sends_no_credentials() {
  WebDavEntry entries[8];
  size_t count = 0;
  bool truncated = false;
  const WebDavClient::Account anonymous{"https://dav.example.com/pub", "", ""};
  WebDavClient::listDirectory(anonymous, "/pub", entries, 8, count, truncated);
  ASSERT_STREQ(sim_http_fetch::lastRequest().basicAuth, "");
}

static void test_status_codes_map_to_errors() {
  WebDavEntry entries[4];
  size_t count = 0;
  bool truncated = false;

  sim_http_fetch::cannedResponse().statusCode = 401;
  ASSERT_TRUE(WebDavClient::listDirectory(koofrAccount(), "/dav/Koofr", entries, 4, count, truncated) ==
              WebDavClient::Error::AUTH_FAILED);

  sim_http_fetch::cannedResponse().statusCode = 403;
  ASSERT_TRUE(WebDavClient::listDirectory(koofrAccount(), "/dav/Koofr", entries, 4, count, truncated) ==
              WebDavClient::Error::AUTH_FAILED);

  sim_http_fetch::cannedResponse().statusCode = 404;
  ASSERT_TRUE(WebDavClient::listDirectory(koofrAccount(), "/dav/Koofr", entries, 4, count, truncated) ==
              WebDavClient::Error::NOT_FOUND);

  sim_http_fetch::cannedResponse().statusCode = 500;
  ASSERT_TRUE(WebDavClient::listDirectory(koofrAccount(), "/dav/Koofr", entries, 4, count, truncated) ==
              WebDavClient::Error::HTTP_FAILED);

  // A plain 200 is accepted too: not every server answers 207.
  sim_http_fetch::cannedResponse().statusCode = 200;
  ASSERT_TRUE(WebDavClient::listDirectory(koofrAccount(), "/dav/Koofr/Books", entries, 4, count, truncated) ==
              WebDavClient::Error::OK);
}

static void test_transport_and_configuration_failures() {
  WebDavEntry entries[4];
  size_t count = 0;
  bool truncated = false;

  ASSERT_TRUE(WebDavClient::listDirectory(WebDavClient::Account{}, "/", entries, 4, count, truncated) ==
              WebDavClient::Error::NO_URL);
  ASSERT_TRUE(WebDavClient::listDirectory(koofrAccount(), "/", nullptr, 0, count, truncated) ==
              WebDavClient::Error::NO_BUFFER);

  sim_http_fetch::transportAvailable() = false;
  ASSERT_TRUE(WebDavClient::listDirectory(koofrAccount(), "/dav/Koofr", entries, 4, count, truncated) ==
              WebDavClient::Error::HTTP_FAILED);
}

static void test_unparsable_body_reports_parse_failure() {
  WebDavEntry entries[4];
  size_t count = 0;
  bool truncated = false;
  sim_http_fetch::cannedResponse().body = "<d:multistatus><d:response>";
  ASSERT_TRUE(WebDavClient::listDirectory(koofrAccount(), "/dav/Koofr", entries, 4, count, truncated) ==
              WebDavClient::Error::PARSE_FAILED);
  ASSERT_EQ(count, static_cast<size_t>(0));
}

static void test_capacity_overflow_is_reported() {
  WebDavEntry entries[2];
  size_t count = 0;
  bool truncated = false;
  const auto error = WebDavClient::listDirectory(koofrAccount(), "/dav/Koofr/Books", entries, 2, count, truncated);
  ASSERT_TRUE(error == WebDavClient::Error::OK);
  ASSERT_TRUE(truncated);
  ASSERT_EQ(count, static_cast<size_t>(1));  // 2 parsed, one of which was the folder itself
}

int main() {
  printf("WebDavClient tests\n");
  RUN_TEST(test_root_path_combines_url_and_root_folder);
  RUN_TEST(test_url_for_path_keeps_origin);
  RUN_TEST(test_lists_members_sorted_without_self);
  RUN_TEST(test_request_shape);
  RUN_TEST(test_anonymous_account_sends_no_credentials);
  RUN_TEST(test_status_codes_map_to_errors);
  RUN_TEST(test_transport_and_configuration_failures);
  RUN_TEST(test_unparsable_body_reports_parse_failure);
  RUN_TEST(test_capacity_overflow_is_reported);

  printf("\n%d passed, %d failed\n", testsPassed, testsFailed);
  return testsFailed == 0 ? 0 : 1;
}
