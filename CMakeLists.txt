cmake_minimum_required(VERSION 3.5)

option(NCW_CLI "Build CLI" ON)

project(ncw)
set(EXEC_NAME ncw-cli)

set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_options(-O1 -g -fsanitize=address -fno-omit-frame-pointer)
else()
    add_compile_options(-O2)
endif()

add_library(${PROJECT_NAME} STATIC
    ncw.cc
    url.cc
    conn.cc
    req.cc
)

if(NCW_CLI)
    add_executable(${EXEC_NAME}
        main.cc
    )
    
    find_package(OpenSSL REQUIRED)
    
    include_directories(${EXEC_NAME}
        ${OPENSSL_INCLUDE_DIR}
    )
    
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
        target_link_libraries(${EXEC_NAME}
    	${PROJECT_NAME}
    	OpenSSL::SSL
    	OpenSSL::Crypto
    	-fsanitize=address
        )
    else()
        target_link_libraries(${EXEC_NAME}
    	${PROJECT_NAME}
    	OpenSSL::SSL
    	OpenSSL::Crypto
        )
    endif()
endif()
