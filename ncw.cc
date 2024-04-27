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

    const Response request(const std::string& url,
	    const Method method,
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

}
