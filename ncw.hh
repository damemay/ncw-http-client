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
        	constexpr uint16_t recv_offset{1024};
        }

	enum class Method {
    	    get,
    	    head,
    	    post,
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
    	        const Method method;
    	        const uint64_t timeout;
    	        const std::string& data;
    	        const std::map<std::string, std::string>& headers;
		Connection& connection;
		const Url& url;

		void send_all(const std::string& data);
		void send_request();
		Response read_response();
		std::string recv_until_terminator(std::string terminator);
		std::string get_data_in_chunks(const std::string& response);
		std::string get_data_with_content_length(const std::string& response, const std::string& length);
    	    
    	    public:
    	        inline Request(const Url& url,
			Connection& connection,
			const Method method = Method::get,
			const std::string& data = {},
			const std::map<std::string, std::string>& headers = {},
			const uint64_t timeout = 60)
		    : url{url}, connection{connection},
		    method{method}, data{data}, headers{headers},
		    timeout{timeout} {}

    	        Response perform();
    	};
    }

    namespace single {
        const Response get(const std::string& url,
    	    const std::string& data = {},
    	    const std::map<std::string, std::string>& headers = {},
    	    const bool follow_redirects = true,
    	    const uint64_t timeout = 60);
    
        const Response head(const std::string& url,
    	    const std::string& data = {},
    	    const std::map<std::string, std::string>& headers = {},
    	    const bool follow_redirects = true,
    	    const uint64_t timeout = 60);
    
        const Response post(const std::string& url,
    	    const std::string& data = {},
    	    const std::map<std::string, std::string>& headers = {},
    	    const bool follow_redirects = true,
    	    const uint64_t timeout = 60);
    }

}

#endif
