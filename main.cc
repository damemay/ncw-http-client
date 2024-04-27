#include "ncw.hh"
#include <iostream>

int main(int argc, char** argv) {
    if(argc < 3) exit(1);

    ncw::Method method;
    if(strcmp(argv[1], "get") == 0) method = ncw::Method::get;
    else if(strcmp(argv[1], "head") == 0) method = ncw::Method::head;
    else if(strcmp(argv[1], "post") == 0) method = ncw::Method::post;

    auto response = ncw::request(argv[2], method);

    std::cout << response.status_code << std::endl;
    for(const auto& header: response.headers)
	std::cout << header.first << ": " << header.second << std::endl;

    std::cout << response.data << std::endl;
}
