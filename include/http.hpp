#pragma once

#include <curl/curl.h>
#include <exception>
#include <string>
#include <vector>

namespace Http {
    namespace Internal {
        size_t WriteCallbackBytes(void* contents, size_t size, size_t nmemb, std::vector<std::byte>* output);
    }

    class HttpException : public std::exception {
    public:
        HttpException(std::string msg) : message(msg) {}

        CURLcode curl_code;
        std::string message;

        const char* what() const noexcept override {
            return message.c_str();
        }
    };

    struct Response {
        long status_code;
        std::string text;
        std::vector<std::byte> bytes;
    };
    
    Response Get(const char* url);
}