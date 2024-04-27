#include "ncw.hh"
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sys/fcntl.h>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <fcntl.h>

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

	void Request::parse_url() {
	    if(this->url.empty())
		throw std::invalid_argument("Cannot perform request with empty URL");
	    auto [port, has_prefix] = get_port(this->url);
	    auto tmp_url {has_prefix ? this->url.substr(this->url.find("//")+2) : this->url};
	    auto hostname {get_hostname(tmp_url)};
	    auto query {get_query(tmp_url)};
	    this->parsed_url = Url{url, hostname, port, query};
	}

	int Request::connect_socket() {
	    struct addrinfo* info {nullptr};
	    int result {0};
	    int fd {0};
	    struct addrinfo hints {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	    };
	    if((result = getaddrinfo(this->parsed_url.hostname.c_str(),
			this->parsed_url.port.c_str(), &hints, &info)) != 0)
		throw std::runtime_error(gai_strerror(result));

	    struct addrinfo* iter;
	    for(iter = info; iter != nullptr; iter = iter->ai_next) {
		if((fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1)
		    continue;
		if(connect(fd, info->ai_addr, info->ai_addrlen) == -1)
		    throw std::runtime_error(strerror(errno));
		break;
	    }
	    if(info == nullptr) throw std::runtime_error("Connection is null");
	    freeaddrinfo(info);
	    return fd;
	}

	static std::string parse_method(Method method) {
	    switch(method) {
		case Method::get:   return "GET"; break;
		case Method::head:  return "HEAD"; break;
		case Method::post:  return "POST"; break;
	    }
	}

	static void send_all(int fd, std::string data) {
	    size_t total {0};
	    size_t size {data.size()};
	    size_t left {size};
	    size_t ret {0};
	    while(total < size) {
		ret = send(fd, data.data()+total, left, 0);
		if(ret == -1) throw std::runtime_error(strerror(errno));
		total += ret;
		left -= ret;
	    }
	}

	void Request::send_request(int fd) {
	    std::string message;
	    message += parse_method(this->method)
		+ " " + this->parsed_url.query
		+ " HTTP/1.1" + std::string(http::newline);
	    message += "Host: " + this->parsed_url.hostname + std::string(http::newline);
	    message += "User-Agent: " + std::string(http::user_agent) + std::string(http::newline);
	    if(!this->headers.empty()) {
		for(const auto& header: headers) {
		    message += header.first + ": " + header.second + std::string(http::newline);
		}
	    }
	    if(this->method != Method::head && !this->data.empty()) {
		message += "Content-Length: " + std::to_string(this->data.size()) + std::string(http::terminator);
		message += this->data + std::string(http::newline);
	    }
	    message += std::string(http::newline);
	    send_all(fd, message);
	}

	static int poll_event(int fd, int timeout, short event) {
	    struct pollfd pfd[1];
	    pfd[0].fd = fd;
	    pfd[0].events = event;
	    int ret = poll(pfd, 1, timeout*1000);
	    if(ret == 0) throw std::runtime_error("Polling timeout"); //timeout
	    else if(ret == -1) throw std::runtime_error(strerror(errno));
	    return pfd[0].revents & event;
	}

	static std::string recv_until_terminator(int fd, std::string terminator, int timeout) {
	    size_t b_off = 0;
	    int recvd = 0;
	    std::vector<char> buffer(http::recv_offset);
	    std::string string;
	    fcntl(fd, F_SETFL, O_NONBLOCK);
	    do {
		if(poll_event(fd, timeout, POLLIN)) {
		    if((recvd = recv(fd, &buffer[b_off], http::recv_offset, 0)) == 0) {
			throw std::runtime_error("Peer closed connection");
		    } else if(recvd == -1) {
			if(errno == EWOULDBLOCK) recvd = http::recv_offset;
			else throw std::runtime_error(strerror(errno));
		    }
		    b_off += recvd;
		    string = std::string(buffer.begin(), buffer.end());
		    buffer.resize(buffer.size() + recvd);
		}
	    } while(string.find(terminator) == std::string::npos);
	    const int flags = fcntl(fd, F_GETFL, 0);
	    fcntl(fd, F_SETFL, flags^O_NONBLOCK);
	    return string;
	}

	static uint16_t get_status_code(std::string line) {
	    size_t st {0};
	    if((st = line.find_first_of(' ')) == std::string::npos) return 0;
	    return std::stoi(line.substr(st));
	}

	static std::pair<std::map<std::string, std::string>, uint16_t> parse_headers_status(std::string response) {
	    std::map<std::string, std::string> headers;
	    uint16_t status_code;
	    size_t nl{0};
	    while(true) {
		size_t nnl {response.find(http::newline, nl)};
		size_t len {nnl-nl};
		if(len == 0) break;
		auto line = response.substr(nl, len);
		size_t sep{0};
		if((sep = line.find(':')) != std::string::npos)
		    headers[line.substr(0,sep)] = line.substr(sep+2);
		else 
		    status_code = get_status_code(line);
		nl = nnl+2;
	    };
	    return std::make_pair(headers, status_code);
	}

	static std::string get_data_with_content_length(int fd, int timeout, const std::string& response, const std::string& length) {
	    long long content_length = std::stoi(length);
	    size_t pos = response.find(http::terminator);
	    assert(pos != std::string::npos);
	    std::string data = response.substr(pos+4);
	    long long remaining = content_length - data.size();
	    if(remaining <= 0) return data;
	    if(poll_event(fd, timeout, POLLIN)) {
		std::vector<char> buffer(remaining);
		size_t recvd {0};
		if((recvd = recv(fd, &buffer[0], remaining, 0)) == 0)
	    	    throw std::runtime_error("Peer closed connection");
		else if(recvd == -1)
		    throw std::runtime_error(strerror(errno));
		data += std::string(buffer.begin(), buffer.end());
	    }
	    return data;
	}

	static std::pair<std::string, bool> read_all_from_buf(const std::string& response) {
	    size_t pos = response.find(http::terminator);
	    assert(pos != std::string::npos);
	    std::string data = response.substr(pos+4);
	    size_t end{};
	    long long size = std::stoll(data, &end, 16);
	    end += 2;
	    size_t next = data.find(http::newline);
	    long long len = next-end;
	    if(size-len <= 0 || len <= 0) {
		size_t terminator = data.find(http::chunk_terminator);
		if(terminator == std::string::npos)
		    return std::make_pair(data, false);
		return std::make_pair(data.substr(end, size-len), true);
	    }
	    return std::make_pair(data, false);
	}

	static std::string parse_chunks(std::string& data) {
	    size_t pos {0};
	    std::string string{};
	    while((pos = data.find(http::newline)) != std::string::npos) {
		std::string in_str = data.substr(0, pos);
		try {
		    std::stoll(in_str, nullptr, 16);
		} catch(std::invalid_argument const& e) {
		    string += in_str;
		}
		data.erase(0, pos+http::newline.length());
	    }
	    return string;
	}

	static std::string get_data_in_chunks(int fd, int timeout, const std::string& response) {
	    auto [data, read_whole] = read_all_from_buf(response);
	    if(read_whole) return data;
	    data += recv_until_terminator(fd, std::string(http::chunk_terminator), timeout);
	    data.shrink_to_fit();
	    return parse_chunks(data);
	}

	Response Request::read_response(int fd) {
	    auto response = recv_until_terminator(fd, std::string(http::terminator), this->timeout);
	    auto [headers, status] = parse_headers_status(response);
	    if(status == 0) throw std::runtime_error("No HTTP status code found");
	    if(this->method == Method::head) return Response{"", status, headers};
	    std::string data;
	    if(headers.find("Transfer-Encoding") != headers.end())
		data = get_data_in_chunks(fd, this->timeout, response);
	    else if(headers.find("Content-Length") != headers.end())
		data = get_data_with_content_length(fd, this->timeout, response, headers.at("Content-Length"));
	    return Response{data, status, headers};
	};

	Response Request::perform() {
	    parse_url();
	    int fd {connect_socket()};
	    send_request(fd);
	    return read_response(fd);
	}
    }

    const Response request(const std::string& url,
	    const Method method,
	    const std::string& data,
	    const std::map<std::string, std::string>& headers,
	    const bool follow_redirects,
	    const uint64_t timeout) {
	if(follow_redirects) {
	    Response inner_response {};
	    bool redirect = false;
	    std::string inner_url = url;
	    do {
		inner_response = inner::Request{inner_url, method, data, headers, timeout}.perform();
		if(inner_response.status_code >= 300 && inner_response.status_code <= 308) {
		    if(auto location = inner_response.headers.find("Location");
			    location != inner_response.headers.end()) {
			inner_url = location->second;
			redirect = true;
		    }
		} else redirect = false;
	    } while(redirect);
	    return inner_response;
	}
	return inner::Request{url, method, data, headers, timeout}.perform();
    }

}
