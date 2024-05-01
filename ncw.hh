#ifndef NCW_H_
#define NCW_H_

#include <cstdint>
#include <string_view>
#include <string>
#include <map>
#include <openssl/ssl.h>

namespace ncw {

    struct Response {
	std::string data;
	uint16_t status_code;
	std::map<std::string, std::string> headers;
    };

    namespace inner {

        namespace http {
	    constexpr std::string_view user_agent{"ncw-http-client"};
    	    constexpr std::string_view newline{"\r\n"};
	    constexpr std::string_view terminator{"\r\n\r\n"};
	    constexpr std::string_view chunk_terminator{"0\r\n\r\n"};
	    constexpr std::string_view prefix_http{"http://"};
	    constexpr std::string_view prefix_https{"https://"};
	    constexpr uint8_t def_timeout{2};
	    constexpr uint16_t recv_offset{1024};
        }

	enum class Method {
    	    get,
    	    head,
    	    post,
	    put,
	    patch,
	    delete_,
	    options,
    	};

	struct Url {
	    std::string url;
	    std::string hostname;
	    std::string port;
	    std::string query;
	    
	    static Url parse(const std::string& url);
	};

	struct Connection {
	    int fd {0};
	    bool is_ssl {false};
	    SSL* ssl {nullptr};
	    SSL_CTX* ssl_ctx {nullptr};

	    Connection(bool init_openssl=false);
	    Connection(const std::string& hostname,
		    const std::string& port,
		    bool init_openssl=false);

	    ~Connection();
	    Connection(const Connection&) = delete;
	    Connection& operator=(const Connection&) = delete;

	    void connect_socket(const std::string& hostname, const std::string& port);
	    bool is_openssl_error_retryable(int return_code);

	    private:
		void init_openssl_lib();
		void init_openssl_connection();
		void handle_openssl_error();
	};

	class Request {
    	    private:
    	        const Method method_;
    	        const uint64_t timeout_;
    	        const std::string& data_;
    	        const std::map<std::string, std::string>& headers_;
    	        const std::map<std::string, std::string>& cookies_;
		Connection& connection_;
		const Url& url_;

		void send_all(const std::string& data);
		void send_request();
		Response read_response();
		std::string recv_until_terminator(std::string terminator);
		std::string get_data_in_chunks(const std::string& response);
		std::string get_data_with_content_length(const std::string& response, const std::string& length);
		std::pair<std::map<std::string, std::string>, uint16_t> parse_headers_status(std::string response);
    	    
    	    public:
    	        inline Request(const Url& url,
			Connection& connection,
			const Method method = Method::get,
			const std::string& data = {},
			const std::map<std::string, std::string>& headers = {},
			const std::map<std::string, std::string>& cookies = {},
			const uint64_t timeout = http::def_timeout)
		    : url_{url}, connection_{connection},
		    method_{method}, data_{data}, headers_{headers},
		    cookies_{cookies}, timeout_{timeout} {}

    	        Response perform();
    	};
    }

#define NCW_METHODS_DECLARATION \
const Response GET(const std::string& url, \
    const std::string& data = {}, \
    const std::map<std::string, std::string>& headers = {}, \
    const std::map<std::string, std::string>& cookies = {}, \
    const bool follow_redirects = true, \
    const uint64_t timeout = inner::http::def_timeout); \
const Response HEAD(const std::string& url, \
    const std::map<std::string, std::string>& headers = {}, \
    const std::map<std::string, std::string>& cookies = {}, \
    const bool follow_redirects = true, \
    const uint64_t timeout = inner::http::def_timeout); \
const Response POST(const std::string& url, \
    const std::string& data = {}, \
    const std::map<std::string, std::string>& headers = {}, \
    const std::map<std::string, std::string>& cookies = {}, \
    const bool follow_redirects = true, \
    const uint64_t timeout = inner::http::def_timeout); \
const Response PUT(const std::string& url, \
    const std::string& data = {}, \
    const std::map<std::string, std::string>& headers = {}, \
    const std::map<std::string, std::string>& cookies = {}, \
    const bool follow_redirects = true, \
    const uint64_t timeout = inner::http::def_timeout); \
const Response PATCH(const std::string& url, \
    const std::string& data = {}, \
    const std::map<std::string, std::string>& headers = {}, \
    const std::map<std::string, std::string>& cookies = {}, \
    const bool follow_redirects = true, \
    const uint64_t timeout = inner::http::def_timeout); \
const Response DELETE(const std::string& url, \
    const std::string& data = {}, \
    const std::map<std::string, std::string>& headers = {}, \
    const std::map<std::string, std::string>& cookies = {}, \
    const bool follow_redirects = true, \
    const uint64_t timeout = inner::http::def_timeout); \
const Response OPTIONS(const std::string& url, \
    const std::map<std::string, std::string>& headers = {}, \
    const std::map<std::string, std::string>& cookies = {}, \
    const bool follow_redirects = true, \
    const uint64_t timeout = inner::http::def_timeout);

    namespace single {
	NCW_METHODS_DECLARATION
    }

    class Session {
	private:
	    uint64_t timeout_ {inner::http::def_timeout};
	    bool follow_redirects_ {true};
	    inner::Url url_ {};
	    std::string data_ {};
    	    std::map<std::string, std::string> headers_ {};
	    std::map<std::string, std::string> cookies_ {};
	    inner::Connection connection_ {true};

	    void parse_cookies(const Response& response);

	public:
	    inline Session(std::string data = {},
		    std::map<std::string, std::string> headers = {},
		    std::map<std::string, std::string> cookies = {},
		    uint64_t timeout = inner::http::def_timeout,
		    bool follow_redirects = true)
		: data_{data}, headers_{headers}, cookies_{cookies},
		timeout_{timeout}, follow_redirects_{follow_redirects} {}
	    ~Session() = default;

	    inline const std::map<std::string, std::string>& get_cookies() const { return cookies_; }

	    inline void set_data(std::string data) { data_ = data; }
	    inline void set_headers(std::map<std::string, std::string> headers) { headers_ = headers; }
	    inline void set_cookies(std::map<std::string, std::string> cookies) { cookies_ = cookies; }

	    inline void clear_data() { data_.clear(); }
	    inline void clear_header() { headers_.clear(); }
	    inline void clear_cookie() { cookies_.clear(); }

	    inline void add_headers(std::map<std::string, std::string> headers) { for(auto& header: headers) headers_.insert_or_assign(header.first, header.second); }
	    inline void add_cookies(std::map<std::string, std::string> cookies) { for(auto& cookie: cookies) cookies_.insert_or_assign(cookie.first, cookie.second); }


	    NCW_METHODS_DECLARATION
    };

}

#endif
