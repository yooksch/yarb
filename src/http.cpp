#include "http.hpp"
#include <cassert>
#include <string>
#include <format>

namespace Http {

    namespace Internal {
        size_t WriteCallbackBytes(void *contents, size_t size, size_t nmemb, std::vector<std::byte>* output) {
            output->insert(
                output->end(),
                static_cast<std::byte*>(contents),
                static_cast<std::byte*>(contents) + size * nmemb
            );
            return size * nmemb;
        }
    }

    Response Get(const char* url) {
        auto curl = curl_easy_init();
        assert(curl); // Ensure curl init was successful

        std::vector<std::byte> bytes {};
        long status_code = 0;

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Internal::WriteCallbackBytes);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bytes);

        if (CURLcode code = curl_easy_perform(curl); code != CURLE_OK) {
            throw HttpException(std::format("CURL Request returned code {}", (int)code));
        } else {
            curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &status_code);
        }

        return Response {
            .status_code = status_code,
            .text = std::string(reinterpret_cast<char*>(bytes.data()), bytes.size()),
            .bytes = bytes
        };
    }

}