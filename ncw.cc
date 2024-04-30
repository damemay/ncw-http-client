#include "ncw.hh"
#include <cstdint>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <fcntl.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace ncw {

    static const Response request(inner::Url& parsed_url,
	    const inner::Method method,
	    const std::string& data,
	    const std::map<std::string, std::string>& headers,
	    const std::map<std::string, std::string>& cookies,
	    inner::Connection& connection,
	    const bool follow_redirects,
	    const uint64_t timeout) {

	if(follow_redirects) {
	    Response inner_response {};
	    bool redirect = false;
	    std::string prev_port {};
	    do {
		inner_response = inner::Request{parsed_url, connection, method, data, headers, cookies, timeout}.perform();
		if(inner_response.status_code >= 300 && inner_response.status_code <= 308) {
		    if(auto location = inner_response.headers.find("location"); location != inner_response.headers.end()) {
			prev_port = parsed_url.port;
			parsed_url = inner::Url::parse(location->second);
			if(parsed_url.port != prev_port)
			    connection.connect_socket(parsed_url.hostname, parsed_url.port);
			redirect = true;
		    }
		} else redirect = false;
	    } while(redirect);
	    return inner_response;
	}
	return inner::Request{parsed_url, connection, method, data, headers, cookies, timeout}.perform();
    }

    static const Response single_request(const std::string& url,
	    const inner::Method method,
	    const std::string& data,
	    const std::map<std::string, std::string>& headers,
	    const std::map<std::string, std::string>& cookies,
	    const bool follow_redirects,
	    const uint64_t timeout) {
	inner::Connection connection{true};
	inner::Url parsed_url = inner::Url::parse(url);
	connection.connect_socket(parsed_url.hostname, parsed_url.port);

	return request(parsed_url, method, data, headers, cookies, connection, follow_redirects, timeout);
    }

    namespace single {
	const Response GET(const std::string& url,
    	        const std::string& data,
    	        const std::map<std::string, std::string>& headers,
    	        const std::map<std::string, std::string>& cookies,
    	        const bool follow_redirects,
    	        const uint64_t timeout) {
    	    return single_request(url, inner::Method::get, data, headers, cookies, follow_redirects, timeout);
    	}

    	const Response HEAD(const std::string& url,
    	        const std::map<std::string, std::string>& headers,
    	        const std::map<std::string, std::string>& cookies,
    	        const bool follow_redirects,
    	        const uint64_t timeout) {
    	    return single_request(url, inner::Method::head, {}, headers, cookies, follow_redirects, timeout);
    	}

    	const Response POST(const std::string& url,
    	        const std::string& data,
    	        const std::map<std::string, std::string>& headers,
    	        const std::map<std::string, std::string>& cookies,
    	        const bool follow_redirects,
    	        const uint64_t timeout) {
    	    return single_request(url, inner::Method::post, data, headers, cookies, follow_redirects, timeout);
    	}

        const Response PUT(const std::string& url,
		const std::string& data,
    	    	const std::map<std::string, std::string>& headers,
    	        const std::map<std::string, std::string>& cookies,
    	    	const bool follow_redirects,
    	    	const uint64_t timeout) {
    	    return single_request(url, inner::Method::put, data, headers, cookies, follow_redirects, timeout);
	}

        const Response PATCH(const std::string& url,
		const std::string& data,
    	    	const std::map<std::string, std::string>& headers,
    	        const std::map<std::string, std::string>& cookies,
    	    	const bool follow_redirects,
    	    	const uint64_t timeout) {
    	    return single_request(url, inner::Method::patch, data, headers, cookies, follow_redirects, timeout);
	}

        const Response DELETE(const std::string& url,
		const std::string& data,
    	    	const std::map<std::string, std::string>& headers,
    	        const std::map<std::string, std::string>& cookies,
    	    	const bool follow_redirects,
    	    	const uint64_t timeout) {
	    return single_request(url, inner::Method::delete_, data, headers, cookies, follow_redirects, timeout);
	}

        const Response OPTIONS(const std::string& url,
		const std::map<std::string, std::string>& headers,
    	        const std::map<std::string, std::string>& cookies,
    	    	const bool follow_redirects,
    	    	const uint64_t timeout) {
    	    return single_request(url, inner::Method::options, {}, headers, cookies, follow_redirects, timeout);
	}
    }

#define NCW_METHODS_SESSION_DEFINITION \
auto p_url = inner::Url::parse(url); \
if(p_url.hostname != url_.hostname || p_url.port != url_.port) { \
    connection_.connect_socket(p_url.hostname, p_url.port); \
    url_ = std::move(p_url); \
} \
if(!headers_.empty()) headers_ = headers; \
if(!cookies_.empty()) cookies_ = cookies;

#define NCW_METHODS_SESSION_DEFINITION_DATA \
if(!data_.empty()) data_ = data;

    const Response Session::GET(const std::string& url,
	    const std::string& data,
            const std::map<std::string, std::string>& headers,
            const std::map<std::string, std::string>& cookies,
            const bool follow_redirects,
            const uint64_t timeout) {
	NCW_METHODS_SESSION_DEFINITION
	NCW_METHODS_SESSION_DEFINITION_DATA
        return request(url_, inner::Method::get, data_, headers_, cookies_, connection_, follow_redirects_, timeout_);
    }
    
    const Response Session::HEAD(const std::string& url,
            const std::map<std::string, std::string>& headers,
            const std::map<std::string, std::string>& cookies,
            const bool follow_redirects,
            const uint64_t timeout) {
	NCW_METHODS_SESSION_DEFINITION
        return request(url_, inner::Method::head, data_, headers_, cookies_, connection_, follow_redirects_, timeout_);
    }
    
    const Response Session::POST(const std::string& url,
            const std::string& data,
            const std::map<std::string, std::string>& headers,
            const std::map<std::string, std::string>& cookies,
            const bool follow_redirects,
            const uint64_t timeout) {
	NCW_METHODS_SESSION_DEFINITION
	NCW_METHODS_SESSION_DEFINITION_DATA
        return request(url_, inner::Method::post, data_, headers_, cookies_, connection_, follow_redirects_, timeout_);
    }
    
    const Response Session::PUT(const std::string& url,
    	    const std::string& data,
	    const std::map<std::string, std::string>& headers,
            const std::map<std::string, std::string>& cookies,
   	    const bool follow_redirects,
	    const uint64_t timeout) {
	NCW_METHODS_SESSION_DEFINITION
	NCW_METHODS_SESSION_DEFINITION_DATA
        return request(url_, inner::Method::put, data_, headers_, cookies_, connection_, follow_redirects_, timeout_);
    }
    
    const Response Session::PATCH(const std::string& url,
	    const std::string& data,
    	    const std::map<std::string, std::string>& headers,
            const std::map<std::string, std::string>& cookies,
	    const bool follow_redirects,
	    const uint64_t timeout) {
	NCW_METHODS_SESSION_DEFINITION
	NCW_METHODS_SESSION_DEFINITION_DATA
        return request(url_, inner::Method::patch, data_, headers_, cookies_, connection_, follow_redirects_, timeout_);
    }
    
    const Response Session::DELETE(const std::string& url,
	    const std::string& data,
    	    const std::map<std::string, std::string>& headers,
            const std::map<std::string, std::string>& cookies,
	    const bool follow_redirects,
	    const uint64_t timeout) {
	NCW_METHODS_SESSION_DEFINITION
	NCW_METHODS_SESSION_DEFINITION_DATA
        return request(url_, inner::Method::delete_, data_, headers_, cookies_, connection_, follow_redirects_, timeout_);
    }
    
    const Response Session::OPTIONS(const std::string& url,
	    const std::map<std::string, std::string>& headers,
            const std::map<std::string, std::string>& cookies,
    	    const bool follow_redirects,
	    const uint64_t timeout) {
	NCW_METHODS_SESSION_DEFINITION
        return request(url_, inner::Method::options, data_, headers_, cookies_, connection_, follow_redirects_, timeout_);
    }

}
