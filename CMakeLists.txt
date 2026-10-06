cmake_minimum_required(VERSION 3.16)

# Tên dự án và ngôn ngữ
project(eda_dag_engine VERSION 1.0 LANGUAGES CXX)

# Ép buộc sử dụng chuẩn C++17 (hoặc C++20 nếu muốn)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Mặc định chuyển sang mode Release nếu người dùng không chỉ định
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# Tìm thư viện Threads (pthread) của hệ điều hành
find_package(Threads REQUIRED)

# Khai báo các file nguồn (Source files)
set(SOURCES
    src/main.cpp
    src/CircuitDAG.cpp
    src/BenchParser.cpp
    src/EventDrivenEngine.cpp
)

# Tạo file thực thi (Executable)
add_executable(${PROJECT_NAME} ${SOURCES})

# Khai báo thư mục chứa file Header (.hpp)
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Cấu hình Flag tối ưu hóa cho Compiler (GCC / Clang)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:-O3 -march=native -flto -Wall -Wextra>
        $<$<CONFIG:Debug>:-g -O0 -Wall -Wextra>
    )
    
    # Bật Link Time Optimization (LTO) ở bản Release để tối ưu liên kết hàm
    set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# Liên kết thư viện Pthread
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)