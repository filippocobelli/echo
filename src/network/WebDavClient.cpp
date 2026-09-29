#include "WebDavClient.h"

#include <Logging.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <utility>

#include "AppVersion.h"
#include "util/UrlUtils.h"

#ifdef SIMULATOR
#include <SimHttpFetch.h>

#include <map>
#else
#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <base64.h>

#include "network/WifiPowerSaveGuard.h"
#endif

namespace {
// Ask only for the three properties the browser renders. Requesting <allprop>
// would multiply the response size for no benefit, and this device streams the
// body through a parser rather than buffering it.
constexpr char PROPFIND_BODY[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<D:propfind xmlns:D=\"DAV:\"><D:prop>"
    "<D:resourcetype/><D:getcontentlength/><D:displayname/>"
    "</D:prop></D:propfind>";
constexpr char CONTENT_TYPE_XML[] = "application/xml; charset=\"utf-8\"";
constexpr int HTTP_STATUS_OK = 200;
constexpr int HTTP_STATUS_MULTI_STATUS = 207;
constexpr int HTTP_STATUS_UNAUTHORIZED = 401;
constexpr int HTTP_STATUS_FORBIDDEN = 403;
constexpr int HTTP_STATUS_NOT_FOUND = 404;

#ifndef SIMULATOR
constexpr size_t PROPFIND_BODY_LENGTH = sizeof(PROPFIND_BODY) - 1;
constexpr int32_t HTTP_CONNECT_TIMEOUT_MS = 10000;
constexpr uint16_t HTTP_RESPONSE_TIMEOUT_MS = 15000;
constexpr uint32_t HTTPS_HANDSHAKE_TIMEOUT_SECONDS = 10;
#endif

WebDavClient::Error errorForStatus(const int status) {
  if (status == HTTP_STATUS_MULTI_STATUS || status == HTTP_STATUS_OK) return WebDavClient::Error::OK;
  if (status == HTTP_STATUS_UNAUTHORIZED || status == HTTP_STATUS_FORBIDDEN) return WebDavClient::Error::AUTH_FAILED;
  if (status == HTTP_STATUS_NOT_FOUND) return WebDavClient::Error::NOT_FOUND;
  return WebDavClient::Error::HTTP_FAILED;
}

// Folders first, then names in case-insensitive order — same ordering rule the
// local file browser uses, so the two screens feel identical.
bool entryLess(const WebDavEntry& lhs, const WebDavEntry& rhs) {
  if (lhs.isCollection != rhs.isCollection) return lhs.isCollection;
  return strcasecmp(lhs.name.c_str(), rhs.name.c_str()) < 0;
}

// Drop the collection's own <response>, which a Depth:1 reply always includes.
// Comparison is on the decoded path so a differently-escaped href still matches.
size_t dropSelfEntry(WebDavEntry* entries, const size_t count, const std::string& requestedPath) {
  const std::string requested = WebDavPath::decode(WebDavPath::normalize(requestedPath));
  size_t kept = 0;
  for (size_t i = 0; i < count; i++) {
    if (WebDavPath::decode(entries[i].path) == requested) continue;
    if (kept != i) entries[kept] = std::move(entries[i]);
    kept++;
  }
  for (size_t i = kept; i < count; i++) {
    entries[i] = WebDavEntry{};
  }
  return kept;
}
}  // namespace

std::string WebDavClient::rootPathFor(const Account& account, const std::string& rootFolder) {
  if (account.url.empty()) return "/";
  const std::string basePath = WebDavPath::normalize(WebDavPath::stripOrigin(UrlUtils::ensureProtocol(account.url)));
  if (rootFolder.empty()) return basePath;
  return WebDavPath::join(basePath, WebDavPath::encode(rootFolder));
}

std::string WebDavClient::urlForPath(const Account& account, const std::string& path) {
  return UrlUtils::buildUrl(account.url, WebDavPath::normalize(path));
}

WebDavClient::Error WebDavClient::listDirectory(const Account& account, const std::string& path, WebDavEntry* entries,
                                                const size_t capacity, size_t& outCount, bool& outTruncated) {
  outCount = 0;
  outTruncated = false;

  if (!entries || capacity == 0) return Error::NO_BUFFER;
  if (account.url.empty()) return Error::NO_URL;

  const std::string url = urlForPath(account, path);
  LOG_DBG("WEBDAV", "PROPFIND %s", url.c_str());

  WebDavParser parser(entries, capacity);
  if (parser.error()) return Error::NO_BUFFER;

#ifdef SIMULATOR
  // The simulator has no HTTPClient that can issue a custom verb, so talk to
  // sim_http_fetch directly. It shells out to curl (real server) or serves a
  // canned response from $CROSSPOINT_SIM_HTTP_MOCK_ROOT (offline mock).
  const std::map<std::string, std::string> headers = {{"Depth", "1"}, {"Content-Type", CONTENT_TYPE_XML}};
  const std::string basicAuth = account.username.empty() ? "" : account.username + ":" + account.password;

  sim_http_fetch::Response response;
  if (!sim_http_fetch::fetch(url, "PROPFIND", headers, basicAuth, PROPFIND_BODY, response)) {
    LOG_ERR("WEBDAV", "PROPFIND transport failed");
    return Error::HTTP_FAILED;
  }
  const Error statusError = errorForStatus(response.statusCode);
  if (statusError != Error::OK) {
    LOG_ERR("WEBDAV", "PROPFIND failed: %d", response.statusCode);
    return statusError;
  }
  parser.parse(response.body.c_str(), response.body.size());
#else
  WifiPowerSaveGuard wifiPowerSaveGuard;
  (void)wifiPowerSaveGuard;

  std::unique_ptr<NetworkClient> client;
  if (UrlUtils::isHttpsUrl(UrlUtils::ensureProtocol(account.url))) {
    auto* secureClient = new (std::nothrow) NetworkClientSecure();
    if (!secureClient) {
      LOG_ERR("WEBDAV", "Failed to allocate secure client");
      return Error::HTTP_FAILED;
    }
    secureClient->setHandshakeTimeout(HTTPS_HANDSHAKE_TIMEOUT_SECONDS);
    secureClient->setInsecure();
    client.reset(secureClient);
  } else {
    auto* plainClient = new (std::nothrow) NetworkClient();
    if (!plainClient) {
      LOG_ERR("WEBDAV", "Failed to allocate client");
      return Error::HTTP_FAILED;
    }
    client.reset(plainClient);
  }

  HTTPClient http;
  http.begin(*client, url.c_str());
  // PROPFIND is not GET, so the default redirect policy would silently drop the
  // verb on a 301. FORCE keeps the method across a permanent redirect, which is
  // how several providers move you from the bare host to the real DAV endpoint.
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setReuse(false);
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_RESPONSE_TIMEOUT_MS);
  http.addHeader("User-Agent", "ECHO-ESP32-" ECHO_VERSION);
  http.addHeader("Depth", "1");
  http.addHeader("Content-Type", CONTENT_TYPE_XML);
  if (!account.username.empty()) {
    const std::string credentials = account.username + ":" + account.password;
    http.addHeader("Authorization", "Basic " + base64::encode(credentials.c_str()));
  }

  // sendRequest() takes a mutable payload pointer but only ever reads from it.
  const int status =
      http.sendRequest("PROPFIND", reinterpret_cast<uint8_t*>(const_cast<char*>(PROPFIND_BODY)), PROPFIND_BODY_LENGTH);
  const Error statusError = errorForStatus(status);
  if (statusError != Error::OK) {
    if (status < 0) {
      LOG_ERR("WEBDAV", "PROPFIND failed: %d (%s)", status, HTTPClient::errorToString(status).c_str());
    } else {
      LOG_ERR("WEBDAV", "PROPFIND failed: %d", status);
    }
    http.end();
    return statusError;
  }

  int writeResult = 0;
  {
    // Scoped so the stream's destructor finalises the parse before we read it.
    WebDavParserStream stream(parser);
    writeResult = http.writeToStream(&stream);
  }
  http.end();

  if (writeResult < 0) {
    LOG_ERR("WEBDAV", "Response read error: %d (%s)", writeResult, HTTPClient::errorToString(writeResult).c_str());
    return Error::HTTP_FAILED;
  }
#endif

  if (parser.error()) {
    LOG_ERR("WEBDAV", "Multistatus parse failed (reason=%d)", static_cast<int>(parser.getErrorReason()));
    return Error::PARSE_FAILED;
  }

  outTruncated = parser.wasTruncated();
  outCount = dropSelfEntry(entries, parser.getEntryCount(), path);
  std::sort(entries, entries + outCount, entryLess);

  LOG_DBG("WEBDAV", "Listed %zu entries (truncated=%d)", outCount, outTruncated ? 1 : 0);
  return Error::OK;
}
