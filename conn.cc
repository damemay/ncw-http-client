#include "ncw.hh"
#include <cerrno>
#include <cstring>
#include <new>
#include <stdexcept>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <fcntl.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace ncw {
    namespace inner {

	void Connection::connect_socket(const std::string& hostname, const std::string& port) {
	    struct addrinfo* info {nullptr};
	    int result {0};
	    struct addrinfo hints {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	    };
	    if((result = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &info)) != 0)
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
	    if(port == "443") init_openssl_connection();
	}

	void Connection::handle_openssl_error() {
	    int err;
	    while((err = ERR_get_error())) {
		throw std::runtime_error(ERR_error_string(err, 0));
	    }
	}

	bool Connection::is_openssl_error_retryable(int return_code) {
	    int err = SSL_get_error(ssl, return_code);
	    switch(err) {
		case SSL_ERROR_NONE:
		    return false;
		case SSL_ERROR_ZERO_RETURN:
		    throw std::runtime_error("Peer closed connection");
		case SSL_ERROR_SYSCALL:
		    throw std::runtime_error(strerror(errno));
		case SSL_ERROR_SSL:
		    throw std::runtime_error("Fatal OpenSSL error");
		default:
		    return true;
	    }
	}

	void Connection::init_openssl_lib() {
	    SSL_library_init();
	    SSL_load_error_strings();
	    OpenSSL_add_ssl_algorithms();
	    ssl_ctx = SSL_CTX_new(TLS_client_method());
	    if(!ssl_ctx) throw std::bad_alloc();
	}

	void Connection::init_openssl_connection() {
	    ssl = SSL_new(ssl_ctx);
	    handle_openssl_error();
	    SSL_set_fd(ssl, this->fd);
	    SSL_connect(ssl);
	    handle_openssl_error();
	    this->is_ssl = true;
	}

	Connection::Connection(bool init_openssl) {
	    if(init_openssl) init_openssl_lib();
	}

	Connection::Connection(const std::string& hostname,
		const std::string& port,
		bool init_openssl) {
	    if(init_openssl) init_openssl_lib();
	    connect_socket(hostname, port);
	}

	Connection::~Connection() {
	    if(ssl) SSL_free(ssl);
	    if(ssl_ctx) SSL_CTX_free(ssl_ctx);
	}

    }
}
