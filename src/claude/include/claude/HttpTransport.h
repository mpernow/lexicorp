#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace claude {
struct HttpResponse {
  long status = 0;   // 0 if no response arrived
  std::string body;  // UTF-8
  std::string error; // set when the request itself failed (DNS, timeout, ...)
};

// Sends one HTTPS request and waits for the response. Implementations must
// allow concurrent calls from several threads.
class IHttpTransport {
public:
  virtual ~IHttpTransport() = default;

  virtual HttpResponse post(const std::string &url,
                            const std::vector<std::string> &headers,
                            const std::string &body) = 0;
};

// IHttpTransport over libcurl, one easy handle per request.
class CurlTransport : public IHttpTransport {
public:
  explicit CurlTransport(
      std::chrono::seconds timeout = std::chrono::seconds{60});

  HttpResponse post(const std::string &url,
                    const std::vector<std::string> &headers,
                    const std::string &body) override;

private:
  std::chrono::seconds mTimeout;
};
} // namespace claude
