#include "ncw.hh"

namespace ncw {
    namespace inner {

	static std::pair<std::string, bool> get_port(const std::string& url) {
	    bool has_prefix {false};
	    std::string port {};
	    size_t pos {0};
	    if(url.find(http::prefix_https) != std::string::npos) {
		port = "443";
		has_prefix = true;
	    } else if(url.find(http::prefix_http) != std::string::npos) {
		port = "80";
		has_prefix = true;
	    } else if((pos = url.find(':')) != std::string::npos) {
		while(std::isdigit(url[pos])) {
		    port += url[pos];
		    pos++;
		}
	    } else {
		port = "80";
	    }
	    return std::make_pair(port, has_prefix);
	}

	static std::string get_hostname(const std::string& url) {
	    size_t pos {0};
	    if((pos = url.find(':')) != std::string::npos 
		    || (pos = url.find('/')) != std::string::npos)
		return url.substr(0, pos);
	    else
		return url;
	}

	static std::string get_query(const std::string& url) {
	    size_t pos {0};
	    if((pos = url.find('/')) != std::string::npos)
		return url.substr(pos);
	    else return "/";
	}

	Url Url::parse(const std::string& url) {
	    if(url.empty())
		throw std::invalid_argument("Cannot perform request with empty URL");
	    auto [port, has_prefix] = get_port(url);
	    auto tmp_url {has_prefix ? url.substr(url.find("//")+2) : url};
	    auto hostname {get_hostname(tmp_url)};
	    auto query {get_query(tmp_url)};
	    return Url{url, hostname, port, query};
	}

    }
}
