#pragma once
// Host-test double for the simulator's HTTP transport. Records the request the
// WebDAV client builds and replays a canned multistatus response, so the real
// WebDavClient code path can be exercised without a server.
#include <map>
#include <string>

namespace sim_http_fetch {

struct Response {
  int statusCode = 0;
  std::string body;
};

struct RecordedRequest {
  std::string url;
  std::string method;
  std::map<std::string, std::string> headers;
  std::string basicAuth;
  std::string body;
};

// Test-controlled: what fetch() should answer, and what it last received.
inline Response& cannedResponse() {
  static Response response;
  return response;
}
inline bool& transportAvailable() {
  static bool available = true;
  return available;
}
inline RecordedRequest& lastRequest() {
  static RecordedRequest request;
  return request;
}

inline bool fetch(const std::string& url, const char* method, const std::map<std::string, std::string>& headers,
                  const std::string& basicAuth, const char* body, Response& out) {
  lastRequest() = RecordedRequest{url, method ? method : "", headers, basicAuth, body ? body : ""};
  if (!transportAvailable()) return false;
  out = cannedResponse();
  return true;
}

}  // namespace sim_http_fetch
