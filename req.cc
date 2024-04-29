#include "ncw.hh"
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
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
		if(!connection.is_ssl)
		    ret = send(connection.fd, data.data()+total, left, 0);
		else {
		    ret = SSL_write(connection.ssl, data.data()+total, left);
		    connection.is_openssl_error_retryable(ret);
		}
		if(ret == -1) throw std::runtime_error(strerror(errno));
		total += ret;
		left -= ret;
	    }
	}

	void Request::send_request() {
	    std::string message;
	    message += parse_method(this->method)
		+ " " + this->url.query
		+ " HTTP/1.1" + std::string(http::newline);
	    message += "Host: " + this->url.hostname + std::string(http::newline);
	    message += "User-Agent: " + std::string(http::user_agent) + std::string(http::newline);
	    if(!this->headers.empty()) {
		for(const auto& header: headers) {
		    message += header.first + ": " + header.second + std::string(http::newline);
		}
	    }
	    if(this->method != Method::head
		    && this->method != Method::delete_
		    && this->method != Method::options
		    && !this->data.empty()) {
		message += "Content-Length: " + std::to_string(this->data.size()) + std::string(http::terminator);
		message += this->data + std::string(http::newline);
	    }
	    message += std::string(http::newline);
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

	std::string Request::recv_until_terminator(std::string terminator) {
	    size_t b_off = 0;
	    int recvd = 0;
	    std::vector<char> buffer(http::recv_offset);
	    std::string string;
	    fcntl(connection.fd, F_SETFL, O_NONBLOCK);
	    if(connection.is_ssl) SSL_set_fd(connection.ssl, connection.fd);
	    do {
		if(poll_event(connection.fd, timeout, POLLIN)) {
		    if(!connection.is_ssl) {
			if((recvd = recv(connection.fd, &buffer[b_off], http::recv_offset, 0)) == 0) {
			    throw std::runtime_error("Peer closed connection");
			} else if(recvd == -1) {
			    if(errno == EWOULDBLOCK) recvd = http::recv_offset;
			    else throw std::runtime_error(strerror(errno));
			}
			b_off += recvd;
			string = std::string(buffer.begin(), buffer.end());
			buffer.resize(buffer.size() + recvd);
		    } else {
			if((recvd = SSL_read(connection.ssl, &buffer[b_off], http::recv_offset)) <= 0) {
			    connection.is_openssl_error_retryable(recvd);
			    continue;
			}
			b_off += recvd;
			string = std::string(buffer.begin(), buffer.end());
			buffer.resize(buffer.size() + recvd);
		    }
		}
	    } while(string.find(terminator) == std::string::npos);
	    const int flags = fcntl(connection.fd, F_GETFL, 0);
	    fcntl(connection.fd, F_SETFL, flags^O_NONBLOCK);
	    if(connection.is_ssl) SSL_set_fd(connection.ssl, connection.fd);
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

	std::string Request::get_data_with_content_length(const std::string& response, const std::string& length) {
	    long long content_length = std::stoi(length);
	    size_t pos = response.find(http::terminator);
	    assert(pos != std::string::npos);
	    std::string data = response.substr(pos+4);
	    long long remaining = content_length - data.size();
	    if(remaining <= 0) return data;
	    if(poll_event(connection.fd, timeout, POLLIN)) {
		std::vector<char> buffer(remaining);
		int recvd {0};
		if(!connection.is_ssl) {
		    if((recvd = recv(connection.fd, &buffer[0], remaining, 0)) == 0)
			throw std::runtime_error("Peer closed connection");
		    else if(recvd == -1)
			throw std::runtime_error(strerror(errno));
		} else {
		    if((recvd = SSL_read(connection.ssl, &buffer[0], remaining)) <= 0)
			connection.is_openssl_error_retryable(recvd);
		}
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

	std::string Request::get_data_in_chunks(const std::string& response) {
	    auto [data, read_whole] = read_all_from_buf(response);
	    if(read_whole) return data;
	    data += recv_until_terminator(std::string(http::chunk_terminator));
	    data.shrink_to_fit();
	    return parse_chunks(data);
	}

	Response Request::read_response() {
	    auto response = recv_until_terminator(std::string(http::terminator));
	    auto [headers, status] = parse_headers_status(response);
	    if(status == 0) throw std::runtime_error("No HTTP status code found");
	    if(this->method == Method::head) return Response{"", status, headers};
	    std::string data;
	    if(headers.find("Transfer-Encoding") != headers.end())
		data = get_data_in_chunks(response);
	    else if(headers.find("Content-Length") != headers.end())
		data = get_data_with_content_length(response, headers.at("Content-Length"));
	    return Response{data, status, headers};
	};

	Response Request::perform() {
	    send_request();
	    return read_response();
	}

    }
}

