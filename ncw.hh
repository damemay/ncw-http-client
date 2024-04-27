#ifndef NCW_H_
#define NCW_H_

#include <cstdint>
#include <string_view>
#include <string>
#include <map>

namespace ncw {

    namespace http {
	constexpr std::string_view user_agent{"ncw-http-client"};
	constexpr std::string_view newline{"\r\n"};
	constexpr std::string_view terminator{"\r\n\r\n"};
	constexpr std::string_view chunk_terminator{"0\r\n\r\n"};
	constexpr std::string_view prefix_http{"http://"};
	constexpr std::string_view prefix_https{"https://"};
	constexpr uint16_t recv_offset{1024};
    }

    enum class Method {
	get,
	head,
	post,
    };

    struct Response {
	std::string data;
	uint16_t status_code;
	std::map<std::string, std::string> headers;
    };

    namespace inner {
	struct Url {
	    std::string url;
	    std::string hostname;
	    std::string port;
	    std::string query;
	};

	class Request {
    	    private:
    	        const Method method;
    	        const uint64_t timeout;
    	        const std::string& url;
    	        const std::string& data;
    	        const std::map<std::string, std::string>& headers;
		Url parsed_url{};

    	        void parse_url();
		int connect_socket();
		void send_request(int fd);
		Response read_response(int fd);
    	    
    	    public:
    	        inline Request(const std::string& url,
    	    	    const Method method = Method::get,
    	    	    const std::string& data = {},
    	    	    const std::map<std::string, std::string>& headers = {},
    	    	    const uint64_t timeout = 60)
    	    	: url{url}, method{method}, data{data}, headers{headers},
    	    	timeout{timeout} {}

    	        Response perform();
    	};
    }


    const Response request(const std::string& url,
	    const Method method = Method::get,
	    const std::string& data = {},
	    const std::map<std::string, std::string>& headers = {},
	    const bool follow_redirects = true,
	    const uint64_t timeout = 60);

}

#endif
