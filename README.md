# ncw-http-client
Not (A) Curl Wrapper HTTP Client for C++17

Yet another HTTP Client implementation of mine with interface resembling Python Requests.

## Sample

```c++
#include <ncw.hh>

int main(int argc, char** argv) {
    // const Response request(const std::string& url,
	//     const Method method = Method::get,
	//     const std::string& data = {},
	//     const std::map<std::string, std::string>& headers = {},
	//     const bool follow_redirects = true,
	//     const uint64_t timeout = 60);
    ncw::Response response = ncw::request("google.com", ncw::Method::get);
    std::cout << response.status_code << std::endl;                     // 200
    std::cout << response.headers["Content-Type"] << std::endl;         // "text/html; charset=ISO-8859-1"
    std::cout << response.data << std::endl;                            // "<!doctype html><html itemscope="" ..."

}
```

## Features

- Simple API
- Custom headers
- Send body data
- Follow redirects
- Connection timeout
- GET, HEAD, POST methods 
- HTTPS connection with OpenSSL

## Planned featurs

- PUT, DELETE methods
- URL encoding POST data and URL parameters
- Basic Auth
- Cookies
- Session with more robust interface
