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

    static const Response request(const std::string& url,
	    const inner::Method method,
	    const std::string& data,
	    const std::map<std::string, std::string>& headers,
	    const bool follow_redirects,
	    const uint64_t timeout) {
	inner::Connection connection{true};
	inner::Url parsed_url = inner::Url::parse(url);
	connection.connect_socket(parsed_url.hostname, parsed_url.port);

	if(follow_redirects) {
	    Response inner_response {};
	    bool redirect = false;
	    std::string prev_port {};
	    do {
		inner_response = inner::Request{parsed_url, connection, method, data, headers, timeout}.perform();
		if(inner_response.status_code >= 300 && inner_response.status_code <= 308) {
		    if(auto location = inner_response.headers.find("Location"); location != inner_response.headers.end()) {
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
	return inner::Request{parsed_url, connection, method, data, headers, timeout}.perform();
    }

    namespace single {
	const Response GET(const std::string& url,
    	        const std::string& data,
    	        const std::map<std::string, std::string>& headers,
    	        const bool follow_redirects,
    	        const uint64_t timeout) {
    	    return request(url, inner::Method::get, data, headers, follow_redirects, timeout);
    	}

    	const Response HEAD(const std::string& url,
    	        const std::map<std::string, std::string>& headers,
    	        const bool follow_redirects,
    	        const uint64_t timeout) {
    	    return request(url, inner::Method::head, {}, headers, follow_redirects, timeout);
    	}

    	const Response POST(const std::string& url,
    	        const std::string& data,
    	        const std::map<std::string, std::string>& headers,
    	        const bool follow_redirects,
    	        const uint64_t timeout) {
    	    return request(url, inner::Method::post, data, headers, follow_redirects, timeout);
    	}

        const Response PUT(const std::string& url,
		const std::string& data,
    	    	const std::map<std::string, std::string>& headers,
    	    	const bool follow_redirects,
    	    	const uint64_t timeout) {
    	    return request(url, inner::Method::put, data, headers, follow_redirects, timeout);
	}

        const Response PATCH(const std::string& url,
		const std::string& data,
    	    	const std::map<std::string, std::string>& headers,
    	    	const bool follow_redirects,
    	    	const uint64_t timeout) {
    	    return request(url, inner::Method::patch, data, headers, follow_redirects, timeout);
	}

        const Response DELETE(const std::string& url,
		const std::string& data,
    	    	const std::map<std::string, std::string>& headers,
    	    	const bool follow_redirects,
    	    	const uint64_t timeout) {
	    return request(url, inner::Method::delete_, data, headers, follow_redirects, timeout);
	}

        const Response OPTIONS(const std::string& url,
		const std::map<std::string, std::string>& headers,
    	    	const bool follow_redirects,
    	    	const uint64_t timeout) {
    	    return request(url, inner::Method::options, {}, headers, follow_redirects, timeout);
	}
    }

}
