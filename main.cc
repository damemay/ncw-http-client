#include "ncw.hh"
#include <iostream>

int main(int argc, char** argv) {
    if(argc < 3) exit(1);

    ncw::Response response;
    if(strcmp(argv[1], "get") == 0)
	response = ncw::single_get(argv[2]);
    else if(strcmp(argv[1], "head") == 0)
	response = ncw::single_head(argv[2]);
    else if(strcmp(argv[1], "post") == 0)
	response = ncw::single_post(argv[2]);

    std::cout << response.status_code << std::endl;
    for(const auto& header: response.headers)
	std::cout << header.first << ": " << header.second << std::endl;

    std::cout << response.data << std::endl;
}
