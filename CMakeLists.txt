cmake_minimum_required(VERSION 3.5)

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
)

add_executable(${EXEC_NAME}
    main.cc
)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_link_libraries(${EXEC_NAME}
	${PROJECT_NAME}
	-fsanitize=address
    )
else()
    target_link_libraries(${EXEC_NAME}
	${PROJECT_NAME}
    )
endif()
