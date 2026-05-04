cmake_minimum_required(VERSION 3.5)
project(odin_ros_driver)

# ==========================================
# ROS 2 Jazzy 版本的环境检测逻辑
# ==========================================
if(DEFINED BUILD_SYSTEM)
    set(ROS_VERSION ${BUILD_SYSTEM})
    message(STATUS "ROS_VERSION: ${ROS_VERSION}")
elseif(DEFINED ENV{ROS_DISTRO})
    if("$ENV{ROS_DISTRO}" MATCHES "foxy|galactic|humble|iron|rolling|jazzy")
        set(ROS_VERSION "ROS2")
    else()
        set(ROS_VERSION "ROS1")
    endif()
elseif(DEFINED ENV{ROS_VERSION})
    if("$ENV{ROS_VERSION}" EQUAL "2")
        set(ROS_VERSION "ROS2")
    else()
        set(ROS_VERSION "ROS1")
    endif()
else()
    if(COMMAND catkin_package)
        set(ROS_VERSION "ROS1")
    elseif(COMMAND ament_package)
        set(ROS_VERSION "ROS2")
    else()
        set(ROS_VERSION "ROS2")
        message(WARNING "Unable to determine ROS version, defaulting to ROS2")
    endif()
endif()

if(ROS_VERSION STREQUAL "ROS2")
    add_definitions(-DROS2)
    message(STATUS "Defining ROS2")
else()
    add_definitions(-DROS1)
    message(STATUS "Defining ROS1")
endif()

message(STATUS "Build system: ${ROS_VERSION}")

# Platform detection
execute_process(
    COMMAND uname -m
    OUTPUT_VARIABLE ARCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(ARCH STREQUAL "x86_64")
    set(TARGET_PLATFORM "x86")
    message(STATUS "Detected x86_64 architecture")
elseif(ARCH MATCHES "arm|aarch64")
    set(TARGET_PLATFORM "arm")
    message(STATUS "Detected ARM architecture: ${ARCH}")
else()
    message(WARNING "Unsupported architecture: ${ARCH}. Using default settings")
    set(TARGET_PLATFORM "unknown")
endif()

# Set library path
set(LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")
message(STATUS "Library directory: ${LIB_DIR}")

# Set library name based on platform
if(TARGET_PLATFORM STREQUAL "arm")
    set(LYD_HOST_API_LIB_NAME "lydHostApi_arm")
else()
    set(LYD_HOST_API_LIB_NAME "lydHostApi_amd")
endif()

# Find precompiled lydHostApi library
find_library(LYD_HOST_API_LIB
    NAMES 
        ${LYD_HOST_API_LIB_NAME}
        lib${LYD_HOST_API_LIB_NAME}.a
        lib${LYD_HOST_API_LIB_NAME}.so
    PATHS ${LIB_DIR}
    NO_DEFAULT_PATH
)

if(LYD_HOST_API_LIB)
    message(STATUS "Found lydHostApi library: ${LYD_HOST_API_LIB}")
else()
    file(GLOB LIB_FILES "${LIB_DIR}/lib${LYD_HOST_API_LIB_NAME}.*")
    if(LIB_FILES)
        message(STATUS "Found library files: ${LIB_FILES}")
        set(LYD_HOST_API_LIB ${LIB_FILES})
    else()
        message(FATAL_ERROR "Could not find precompiled lydHostApi library in ${LIB_DIR}")
    endif()
endif()

# Set common compile options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

# Find common dependencies
find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(PCL REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(OpenSSL REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

include_directories(
    include
    ${EIGEN3_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${PCL_INCLUDE_DIRS}
    ${yaml-cpp_INCLUDE_DIR}
    ${LIBUSB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(COMMON_LIBS
    ${OpenCV_LIBS}
    ${PCL_LIBRARIES}
    ${yaml-cpp_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${LIBUSB_LIBRARIES}
    pthread
    rt
    ${CMAKE_DL_LIBS}
    ${LYD_HOST_API_LIB}
)

# ===== ROS1 Configuration =====
if(ROS_VERSION STREQUAL "ROS1")
    message(FATAL_ERROR "This CMakeLists is optimized for ROS2 Jazzy. Please use ROS2 environment.")

# ===== ROS2 Configuration =====
elseif(ROS_VERSION STREQUAL "ROS2")
    message(STATUS "Configuring for ROS2 build")
    
    find_package(ament_cmake REQUIRED)
    find_package(rclcpp REQUIRED)
    find_package(ament_index_cpp REQUIRED)
    find_package(std_msgs REQUIRED)
    find_package(sensor_msgs REQUIRED)
    find_package(nav_msgs REQUIRED)
    find_package(visualization_msgs REQUIRED)
    find_package(cv_bridge REQUIRED)
    find_package(image_transport REQUIRED)
    find_package(pcl_conversions REQUIRED)
    find_package(message_filters REQUIRED)
    find_package(tf2 REQUIRED)
    find_package(tf2_ros REQUIRED)
    find_package(geometry_msgs REQUIRED)
    find_package(tf2_geometry_msgs REQUIRED)
    
    add_executable(host_sdk_sample 
        src/host_sdk_sample.cpp
        src/yaml_parser.cpp  
        src/rawCloudRender.cpp
        src/camera_pose_visualization.cpp
        )
    target_link_libraries(host_sdk_sample ${COMMON_LIBS} yaml-cpp usb-1.0)
    ament_target_dependencies(host_sdk_sample rclcpp ament_index_cpp std_msgs sensor_msgs nav_msgs visualization_msgs cv_bridge image_transport tf2 tf2_ros geometry_msgs tf2_geometry_msgs)
    
    add_library(pointcloud_depth_converter_ros2 src/pointcloud_depth_converter.cpp)
    target_link_libraries(pointcloud_depth_converter_ros2 ${OpenCV_LIBS} ${PCL_LIBRARIES})
    
    add_library(depth_image_ros2_node_lib src/depth_image_ros2_node.cpp)
    target_link_libraries(depth_image_ros2_node_lib pointcloud_depth_converter_ros2 ${OpenCV_LIBS} ${PCL_LIBRARIES})
    ament_target_dependencies(depth_image_ros2_node_lib rclcpp sensor_msgs std_msgs cv_bridge image_transport pcl_conversions message_filters)

    add_executable(pcd2depth_ros2_node src/pcd2depth_ros2.cpp)
    target_link_libraries(pcd2depth_ros2_node depth_image_ros2_node_lib ${OpenCV_LIBS} ${PCL_LIBRARIES} yaml-cpp)
    ament_target_dependencies(pcd2depth_ros2_node rclcpp sensor_msgs std_msgs cv_bridge image_transport pcl_conversions message_filters)

    add_library(cloud_reprojector_ros2 src/cloud_reprojector.cpp)
    target_compile_definitions(cloud_reprojector_ros2 PRIVATE ROS2)
    target_link_libraries(cloud_reprojector_ros2 ${OpenCV_LIBS} ${PCL_LIBRARIES})

    add_executable(cloud_reprojection_ros2_node src/cloud_reprojection_ros.cpp)
    target_compile_definitions(cloud_reprojection_ros2_node PRIVATE ROS2)
    target_link_libraries(cloud_reprojection_ros2_node cloud_reprojector_ros2 ${OpenCV_LIBS} ${PCL_LIBRARIES} yaml-cpp)
    ament_target_dependencies(cloud_reprojection_ros2_node rclcpp sensor_msgs nav_msgs cv_bridge image_transport pcl_conversions message_filters)

    add_executable(image_overlay_node src/image_overlay_node.cpp)
    target_compile_definitions(image_overlay_node PRIVATE ROS2)
    target_link_libraries(image_overlay_node ${OpenCV_LIBS})
    ament_target_dependencies(image_overlay_node rclcpp sensor_msgs cv_bridge image_transport message_filters)

    # =========================================================================
    # 注意：这里已经删除了 C++ 版本的 pcd_publisher_node 编译代码！
    # 彻底杜绝了 _ZN8eprosima7fastcdr3Cdr9serializeEj 的冲突问题
    # =========================================================================

    install(TARGETS 
        host_sdk_sample 
        pcd2depth_ros2_node
        cloud_reprojection_ros2_node
        image_overlay_node
        EXPORT export_${PROJECT_NAME}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION lib/${PROJECT_NAME}
    )

    # ==========================================
    # Python 脚本的精准路径安装
    # ==========================================
    install(PROGRAMS
        scripts/relocation_node.py
        src/pcd_publisher_node.py     # <--- 已经根据你的实际路径修正成了 src/
        DESTINATION lib/${PROJECT_NAME}
    )
    
    install(FILES package.xml DESTINATION share/${PROJECT_NAME})
    install(DIRECTORY include/ DESTINATION include)
    
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/launch_ROS2")
        install(DIRECTORY launch_ROS2/ DESTINATION share/${PROJECT_NAME}/launch)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config")
        install(DIRECTORY config/ DESTINATION share/${PROJECT_NAME}/config)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/launch")
        install(DIRECTORY launch/ DESTINATION share/${PROJECT_NAME}/launch)
    endif()
    
    ament_export_targets(export_${PROJECT_NAME})
    ament_export_dependencies(
        rclcpp 
        ament_index_cpp
        std_msgs 
        sensor_msgs 
        nav_msgs 
        visualization_msgs
        cv_bridge
        image_transport
        pcl_conversions
        message_filters
        tf2
        tf2_ros
        geometry_msgs
        tf2_geometry_msgs
    )
    
    ament_package()
    message(STATUS "Install targets added")
    
else()
    message(FATAL_ERROR "Invalid ROS_VERSION: ${ROS_VERSION}")
endif()

if(TARGET_PLATFORM STREQUAL "arm")
    set_target_properties(host_sdk_sample PROPERTIES LINK_FLAGS "-Wl,--no-as-needed -Wl,--rpath=${LIB_DIR}")
endif()