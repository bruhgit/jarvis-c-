#include "core/http_client.h"

#include "util/common.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace jarvis {

namespace {

std::filesystem::path tempRoot() {
    return std::filesystem::path(".jarvis-tmp");
}

std::string quoteArg(const std::string& value) {
#ifdef _WIN32
    std::string out = "\"";
    for (char ch : value) {
        if (ch == '"') {
            out += "\\\"";
        } else {
            out.push_back(ch);
        }
    }
    out += "\"";
    return out;
#else
    std::string out = "'";
    for (char ch : value) {
        if (ch == '\'') {
            out += "'\"'\"'";
        } else {
            out.push_back(ch);
        }
    }
    out += "'";
    return out;
#endif
}

std::string uniqueSuffix() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

std::string curlBinary() {
#ifdef _WIN32
    return "curl.exe";
#else
    return "curl";
#endif
}

}  // namespace

HttpResponse HttpClient::send(const HttpRequest& request) const {
    std::filesystem::create_directories(tempRoot());

    const std::string suffix = uniqueSuffix();
    const std::filesystem::path bodyPath = tempRoot() / ("request-" + suffix + ".json");
    const std::filesystem::path responsePath = tempRoot() / ("response-" + suffix + ".txt");
    const std::filesystem::path statusPath = tempRoot() / ("status-" + suffix + ".txt");
    const std::filesystem::path errorPath = tempRoot() / ("stderr-" + suffix + ".txt");

    util::writeTextFile(bodyPath, request.body);

    std::ostringstream command;
    command << curlBinary()
            << " -sS"
            << " -X " << request.method
            << " -o " << quoteArg(responsePath.string())
            << " -w \"%{http_code}\""
            << " --connect-timeout " << request.connect_timeout_seconds
            << " --max-time " << request.timeout_seconds;

    for (const auto& [name, value] : request.headers) {
        command << " -H " << quoteArg(name + ": " + value);
    }

    if (!request.body.empty()) {
        command << " --data-binary " << quoteArg("@" + bodyPath.string());
    }

    command << ' ' << quoteArg(request.url)
            << " > " << quoteArg(statusPath.string())
            << " 2> " << quoteArg(errorPath.string());

    HttpResponse response;
    response.exit_code = std::system(command.str().c_str());

    if (std::filesystem::exists(responsePath)) {
        response.body = util::readTextFile(responsePath);
    }
    if (std::filesystem::exists(errorPath)) {
        response.stderr_output = util::trim(util::readTextFile(errorPath));
    }
    if (std::filesystem::exists(statusPath)) {
        const std::string raw = util::trim(util::readTextFile(statusPath));
        try {
            response.http_status = raw.empty() ? 0 : std::stoi(raw);
        } catch (...) {
            response.http_status = 0;
        }
    }

    std::error_code ignored;
    std::filesystem::remove(bodyPath, ignored);
    std::filesystem::remove(responsePath, ignored);
    std::filesystem::remove(statusPath, ignored);
    std::filesystem::remove(errorPath, ignored);

    return response;
}

}  // namespace jarvis
