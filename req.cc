#include "ncw.hh"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <sys/fcntl.h>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <fcntl.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#ifdef NCW_DEBUG
#include <chrono>
#include <iostream>
#endif

namespace ncw {
    namespace inner {

	static std::string parse_method(Method method) {
	    switch(method) {
		case Method::get:	return "GET"; break;
		case Method::head:  	return "HEAD"; break;
		case Method::post:  	return "POST"; break;
		case Method::put:   	return "PUT"; break;
		case Method::patch:   	return "PATCH"; break;
		case Method::delete_:   return "DELETE"; break;
		case Method::options:   return "OPTIONS"; break;
	    }
	}

	void Request::send_all(const std::string& data) {
	    size_t total {0};
	    size_t size {data.size()};
	    size_t left {size};
	    size_t ret {0};
	    while(total < size) {
		if(!connection_.is_ssl)
		    ret = send(connection_.fd, data.data()+total, left, 0);
		else {
		    ret = SSL_write(connection_.ssl, data.data()+total, left);
		    connection_.is_openssl_error_retryable(ret);
		}
		if(ret == -1) throw std::runtime_error(strerror(errno));
		total += ret;
		left -= ret;
	    }
	}

	void Request::send_request() {
	    std::string message;
	    message += parse_method(method_) + " " + url_.query + " HTTP/1.1" + std::string(http::newline);
	    message += "Host: " + url_.hostname + std::string(http::newline);
	    message += "User-Agent: " + std::string(http::user_agent) + std::string(http::newline);

	    if(!headers_.empty())
		for(const auto& header: headers_)
		    message += header.first + ": " + header.second + std::string(http::newline);

	    if(!cookies_.empty()) {
		message += "Cookie: ";
		for(const auto& cookie: cookies_)
		    message += cookie.first + "=" + cookie.second + "; ";
		message.erase(message.find_last_of(';'));
		message += std::string(http::newline);
	    }

	    if(method_ != Method::head && method_ != Method::delete_ && method_ != Method::options && !data_.empty()) {
		message += "Content-Length: " + std::to_string(data_.size()) + std::string(http::terminator);
		message += data_ + std::string(http::newline);
	    }
	    message += std::string(http::newline);
#ifdef NCW_DEBUG
	    std::cout << message << std::endl;
#endif
	    send_all(message);
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

	static bool find_string_in_vector(const std::vector<char>& haystack, const std::string& needle) {
	    const char* n = needle.c_str();
	    bool found = std::search(haystack.begin(), haystack.end(), n, n+strlen(n)) != haystack.end();
	    return found;
	}

	static int recv_nb(Connection& connection, std::vector<char>& buffer, size_t size, int timeout) {
	    int recvd {0};
	    if(poll_event(connection.fd, timeout, POLLIN)) {
		if(!connection.is_ssl) {
		    if((recvd = recv(connection.fd, &buffer[0], size, 0)) == 0) {
		        throw std::runtime_error("Peer closed connection");
		    } else if(recvd == -1) {
		        if(errno == EWOULDBLOCK) recvd = size;
		        else throw std::runtime_error(strerror(errno));
		    }
		} else {
		    if((recvd = SSL_read(connection.ssl, &buffer[0], size)) <= 0) {
		        connection.is_openssl_error_retryable(recvd);
			recvd = -1;
		    }
		}
	    }
	    return recvd;
	}

	static int recv_b(Connection& connection, std::vector<char>& buffer, size_t size, int timeout) {
	    int recvd {0};
	    if(poll_event(connection.fd, timeout, POLLIN)) {
		if(!connection.is_ssl) {
	    	    if((recvd = recv(connection.fd, &buffer[0], size, 0)) == 0)
	    	        throw std::runtime_error("Peer closed connection");
	    	    else if(recvd == -1)
	    	        throw std::runtime_error(strerror(errno));
	    	} else {
	    	    if((recvd = SSL_read(connection.ssl, &buffer[0], size)) <= 0)
	    		connection.is_openssl_error_retryable(recvd);
	    	}
	    }
	    return recvd;
	}

	std::string Request::recv_until_terminator(std::string terminator) {
	    std::vector<char> buffer(http::recv_offset);
	    fcntl(connection_.fd, F_SETFL, O_NONBLOCK);
	    if(connection_.is_ssl) SSL_set_fd(connection_.ssl, connection_.fd);
	    std::vector<char> tmp_buffer(http::recv_offset);
	    do {
		int recvd = recv_nb(connection_, tmp_buffer, http::recv_offset, timeout_);
		if(recvd == 0 || recvd == -1) continue;
		buffer.insert(buffer.end(), std::begin(tmp_buffer), std::end(tmp_buffer));
	    } while(!find_string_in_vector(tmp_buffer, terminator));
	    const int flags = fcntl(connection_.fd, F_GETFL, 0);
	    fcntl(connection_.fd, F_SETFL, flags^O_NONBLOCK);
	    if(connection_.is_ssl) SSL_set_fd(connection_.ssl, connection_.fd);
	    return std::string(buffer.begin(), buffer.end());
	}

	static uint16_t get_status_code(std::string line) {
	    size_t st {0};
	    if((st = line.find_first_of(' ')) == std::string::npos) return 0;
	    return std::stoi(line.substr(st));
	}

	std::pair<std::map<std::string, std::string>, uint16_t> Request::parse_headers_status(std::string response) {
	    std::map<std::string, std::string> headers;
	    uint16_t status_code;
	    size_t nl{0};
	    size_t nnl{0};
	    size_t len{0};
	    while((nnl = response.find(http::newline, nl)) != std::string::npos) {
		size_t len {nnl-nl};
		if(len == 0) break;
		auto line = response.substr(nl, len);
		size_t sep{0};
		if((sep = line.find(':')) != std::string::npos) {
		    std::string key = line.substr(0,sep);
		    std::string val = line.substr(sep+2);
		    for(auto& c : key) c = std::tolower(c);
		    if(key == "set-cookie")
			if(headers.find("set-cookie") != headers.end())
			    val += "; " + headers.at("set-cookie");
		    headers[key] = val;
		} else 
		    status_code = get_status_code(line);
		nl = nnl+2;
	    };
	    return std::make_pair(headers, status_code);
	}

	std::string Request::get_data_with_content_length(const std::string& response, const std::string& length) {
	    long long content_length = std::stoi(length);
	    size_t pos = response.find(http::terminator);
	    assert(pos != std::string::npos);
	    std::string data = response.substr(pos+4);
	    data.shrink_to_fit();
	    long long remaining = content_length - data.size();
#ifdef NCW_DEBUG
	    std::cout << data.size() << "/" << remaining << std::endl;
#endif
	    if(remaining <= 0) return data;
	    std::vector<char> buffer(remaining);
	    int recvd = recv_b(connection_, buffer, remaining, timeout_);
	    data += std::string(buffer.begin(), buffer.end());
#ifdef NCW_DEBUG
	    data.shrink_to_fit();
	    std::cout << data.size() << "/" << content_length << std::endl;
	    std::cout << data << std::endl;
#endif
	    return data;
	}

	static std::pair<std::string, bool> read_all_from_buf(const std::string& response) {
	    size_t pos = response.find(http::terminator);
	    assert(pos != std::string::npos);
	    std::string data = response.substr(pos+4);
	    size_t end{};
	    long long size;
	    try {
		size = std::stoll(data, &end, 16);
	    	end += 2;
	    } catch(std::invalid_argument const& e) {
		return std::make_pair(data, false);
	    }
	    size_t next = data.find(http::newline);
	    long long len = next-end;
	    if(size-len <= 0 || len <= 0) {
		size_t terminator = data.find(http::chunk_terminator);
		if(terminator == std::string::npos) return std::make_pair(data, false);
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

	std::string Request::get_data_in_chunks(const std::string& response) {
#ifdef NCW_DEBUG
	    auto s {std::chrono::high_resolution_clock::now()};
	    std::cout << ">read_all_from_buf: ";
#endif
	    auto [data, read_whole] = read_all_from_buf(response);
#ifdef NCW_DEBUG
	    auto e {std::chrono::high_resolution_clock::now()};
	    std::chrono::duration<double, std::milli> ms {e - s};
	    std::cout << ms.count() << std::endl;
#endif
	    if(read_whole) return data;
#ifdef NCW_DEBUG
	    s = std::chrono::high_resolution_clock::now();
	    std::cout << ">recv_until_terminator: ";
#endif
	    data += recv_until_terminator(std::string(http::chunk_terminator));
#ifdef NCW_DEBUG
	    e = std::chrono::high_resolution_clock::now();
	    ms = e - s;
	    std::cout << ms.count() << std::endl;
#endif
	    data.shrink_to_fit();
#ifdef NCW_DEBUG
	    s = std::chrono::high_resolution_clock::now();
	    std::cout << ">parse_chunks: ";
#endif
	    auto r = parse_chunks(data);
#ifdef NCW_DEBUG
	    e = std::chrono::high_resolution_clock::now();
	    ms = e - s;
	    std::cout << ms.count() << std::endl;
#endif
	    return r;
	}

	Response Request::read_response() {
#ifdef NCW_DEBUG
	    auto s {std::chrono::high_resolution_clock::now()};
	    std::cout << ">recv_until_terminator: ";
#endif
	    auto response = recv_until_terminator(std::string(http::terminator));
#ifdef NCW_DEBUG
	    auto e {std::chrono::high_resolution_clock::now()};
	    std::chrono::duration<double, std::milli> ms {e - s};
	    std::cout << ms.count() << std::endl;
#endif
	    auto [headers, status] = parse_headers_status(response);
	    if(status == 0) throw std::runtime_error("No HTTP status code found");
	    if(method_ == Method::head || method_ == Method::options)
		return Response{"", status, headers};
	    std::string data;
	    if(headers.find("transfer-encoding") != headers.end()) {
		if(headers.at("transfer-encoding") == "chunked") data = get_data_in_chunks(response);
	    } else if(headers.find("content-length") != headers.end())
		data = get_data_with_content_length(response, headers.at("content-length"));
	    return Response{data, status, headers};
	};

	Response Request::perform() {
	    send_request();
	    return read_response();
	}

    }
}

