#pragma once

#include <map>
#include <string>

namespace jarvis {

struct HttpRequest {
    std::string method = "POST";
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
    int timeout_seconds = 90;
    int connect_timeout_seconds = 15;
};

struct HttpResponse {
    int exit_code = 0;
    int http_status = 0;
    std::string body;
    std::string stderr_output;
};

class HttpClient {
public:
    HttpResponse send(const HttpRequest& request) const;
};

}  // namespace jarvis
