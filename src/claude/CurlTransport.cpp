#include "claude/HttpTransport.h"

#include <curl/curl.h>
#include <memory>
#include <mutex>

namespace claude {
namespace {
// curl_global_init() isn't thread-safe, so it runs once, before the first
// request; requests may then come from any thread.
void initCurl() {
  static std::once_flag once;
  std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

size_t appendToString(char *data, size_t size, size_t count, void *out) {
  static_cast<std::string *>(out)->append(data, size * count);
  return size * count;
}
} // namespace

CurlTransport::CurlTransport(std::chrono::seconds timeout)
    : mTimeout(timeout) {
  initCurl();
}

HttpResponse CurlTransport::post(const std::string &url,
                                 const std::vector<std::string> &headers,
                                 const std::string &body) {
  HttpResponse response;

  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl{curl_easy_init(),
                                                          curl_easy_cleanup};
  if (!curl) {
    response.error = "could not start an HTTP request";
    return response;
  }

  curl_slist *headerList = nullptr;
  for (const auto &header : headers)
    headerList = curl_slist_append(headerList, header.c_str());
  std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headerGuard{
      headerList, curl_slist_free_all};

  CURL *handle = curl.get();
  curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
  curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headerList);
  curl_easy_setopt(handle, CURLOPT_POSTFIELDS, body.data());
  curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE,
                   static_cast<curl_off_t>(body.size()));
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, appendToString);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response.body);
  curl_easy_setopt(handle, CURLOPT_TIMEOUT,
                   static_cast<long>(mTimeout.count()));
  // Signals can't be used for timeouts in a multi-threaded program.
  curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);

  CURLcode result = curl_easy_perform(handle);
  if (result != CURLE_OK) {
    response.error = curl_easy_strerror(result);
    return response;
  }
  curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response.status);
  return response;
}
} // namespace claude
