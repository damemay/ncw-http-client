#include "ncw.hh"
#include <iostream>

int main(int argc, char** argv) {
    if(argc < 3) {
	std::cout << "Usage: " << argv[0] << " <method> <url>" << std::endl;
	std::cout << " Methods: GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS" << std::endl;
	exit(1);
    }

    ncw::Response response;
    if(strcmp(argv[1], "GET") == 0)
	response = ncw::single::GET(argv[2]);
    else if(strcmp(argv[1], "HEAD") == 0)
	response = ncw::single::HEAD(argv[2]);
    else if(strcmp(argv[1], "POST") == 0)
	response = ncw::single::POST(argv[2]);
    else if(strcmp(argv[1], "PUT") == 0)
	response = ncw::single::PUT(argv[2]);
    else if(strcmp(argv[1], "PATCH") == 0)
	response = ncw::single::PATCH(argv[2]);
    else if(strcmp(argv[1], "DELETE") == 0)
	response = ncw::single::DELETE(argv[2]);
    else if(strcmp(argv[1], "OPTIONS") == 0)
	response = ncw::single::OPTIONS(argv[2]);

#ifndef NCW_DEBUG
    std::cout << "HTTP Status code: " << response.status_code << std::endl;
    for(const auto& header: response.headers)
	std::cout << header.first << ": " << header.second << std::endl;

    std::cout << response.data << std::endl;
#endif
}
