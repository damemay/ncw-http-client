# ncw-http-client
Not (A) Curl Wrapper HTTP Client for C++17

Yet another HTTP Client implementation of mine with interface taking after Python Requests. But it's not a libcurl wrapper.

### Why shouldn't I just use [C++ Requests](https://github.com/libcpr/cpr)?

*ncw* introduces minimum number of new types allowing for simple, fast but powerful usage - all in and for C++17 code. Because of that, it's as dependency-free as it can and it doesn't need to conform with design/API of library it's wrapping.

# Sample

```c++
#include <ncw.hh>

int main(int argc, char** argv) {
    ncw::Response response = ncw::single::GET("google.com");
    std::cout << response.status_code << std::endl;                     // 200
    std::cout << response.headers["Content-Type"] << std::endl;         // "text/html; charset=ISO-8859-1"
    std::cout << response.data << std::endl;                            // "<!doctype html><html itemscope="" ..."

}
```

# Features

- Simple API
- Custom headers
- Send body data
- Follow redirects
- Connection timeout
- GET, HEAD, POST, PATCH, PUT, DELETE, OPTIONS methods 
- HTTPS connection with OpenSSL

# Planned featurs

- URL encoding POST and GET parameters
- Basic Auth
- Cookies
- Session with more robust interface
