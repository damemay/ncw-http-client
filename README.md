# ncw-http-client
Not (A) Curl Wrapper HTTP Client for C++17

Yet another HTTP Client implementation of mine with interface taking after Python Requests. But it's not a libcurl wrapper.

### Why shouldn't I just use [C++ Requests](https://github.com/libcpr/cpr)?

*ncw* introduces minimum number of new types allowing for simple, fast but powerful usage - all in and for C++17 code. Because of that, it's as dependency-free as it can and it doesn't need to conform with design/API of library it's wrapping. Although also because of that, it probably will never have the stability of libcurl nor be production-ready.

# Sample

```c++
#include <ncw.hh>

int main(int argc, char** argv) {
    ncw::Response response {ncw::single::GET("google.com")};    // Single API
    std::cout << response.status_code;                          // 200
    std::cout << response.headers["content-type"];              // "text/html;..."
    std::cout << response.data;                                 // "<!doctype html>..."

    ncw::Session session {};                                    // Session API
    response = session.GET("google.com");
    std::cout << session.get_cookies().at("AEC");               // "AQTF6H..."
}
```

# Features

- Simple "single" API
- Session API with cookie support (no attributes handling yet)
- Custom headers
- Send body data
- Follow redirects
- Connection timeout
- GET, HEAD, POST, PATCH, PUT, DELETE, OPTIONS methods 
- HTTPS connection with OpenSSL

# Planned features

- URL encoding POST and GET parameters
- Basic Auth
