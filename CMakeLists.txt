cmake_minimum_required(VERSION 3.18)
project(ImageClassifier)
set(CMAKE_CXX_STANDARD 17)
find_package(Threads)

include_directories("api")
link_directories(api-and-client-lib)
add_executable(classinet command-line-app/src/main_classinet_app.cpp command-line-app/src/LinuxSpecific.cpp command-line-app/src/LinuxSpecific.h)
target_link_libraries(classinet classinet_client_v1.0_ubuntu_20.04)
