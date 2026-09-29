#pragma once
#include <WebDavParser.h>

#include <cstddef>
#include <string>

/**
 * Minimal WebDAV *client* for browsing remote cloud storage (Koofr, Nextcloud,
 * ownCloud, box.com ...). This is the counterpart of WebDAVHandler, which is
 * the server that exposes this device's SD card on the LAN.
 *
 * Only the two verbs a read-only library browser needs are implemented:
 *   - PROPFIND with Depth:1 to list a collection
 *   - GET (through HttpDownloader) to fetch a file
 *
 * Authentication is HTTP Basic. Callers are expected to already be on WiFi.
 */
class WebDavClient {
 public:
  struct Account {
    std::string url;       // e.g. https://app.koofr.net/dav/Koofr
    std::string username;  // may be empty for anonymous shares
    std::string password;  // app-specific password on Koofr
  };

  enum class Error {
    OK = 0,
    NO_URL,        // account has no server URL configured
    NO_BUFFER,     // caller passed no entry storage
    HTTP_FAILED,   // transport error, DNS failure, TLS failure, timeout
    AUTH_FAILED,   // 401/403 — wrong username or password
    NOT_FOUND,     // 404 — folder is gone
    PARSE_FAILED,  // the multistatus body could not be parsed
  };

  /**
   * Absolute server path (percent-encoded, no origin) of the account's root.
   * Combines the path part of the account URL with the user's root folder.
   */
  static std::string rootPathFor(const Account& account, const std::string& rootFolder);

  /** Full request URL for an absolute server path. */
  static std::string urlForPath(const Account& account, const std::string& path);

  /**
   * PROPFIND a collection and fill `entries` with its members.
   *
   * The collection's own <response> — always the first one in a Depth:1 reply —
   * is filtered out, so only children come back. Results are sorted folders
   * first, then case-insensitively by name.
   *
   * @param entries    caller-owned buffer, allocated once and reused
   * @param capacity   number of slots in `entries`
   * @param outCount   number of slots filled
   * @param outTruncated set when the folder held more members than `capacity`
   */
  static Error listDirectory(const Account& account, const std::string& path, WebDavEntry* entries, size_t capacity,
                             size_t& outCount, bool& outTruncated);
};
