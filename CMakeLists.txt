cmake_minimum_required(VERSION 3.5)
project(odin_ros_driver)

# ==========================================
# 1. 全局设置与警告消除
# ==========================================
# 消除 PCL/FLANN 的 CMP0144 警告
if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

# Jazzy 推荐 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-DROS2)

# ==========================================
# 2. 平台与架构检测
# ==========================================
execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
if(ARCH STREQUAL "x86_64")
    set(TARGET_PLATFORM "x86")
    set(LYD_HOST_API_LIB_NAME "lydHostApi_amd")
    message(STATUS "Detected x86_64 architecture")
elseif(ARCH MATCHES "arm|aarch64")
    set(TARGET_PLATFORM "arm")
    set(LYD_HOST_API_LIB_NAME "lydHostApi_arm")
    message(STATUS "Detected ARM architecture")
endif()

# ==========================================
# 3. 查找依赖包 (非 ROS)
# ==========================================
find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(PCL REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(OpenSSL REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# 查找驱动静态库 (对应你的 lib/ 目录)
find_library(LYD_HOST_API_LIB
    NAMES ${LYD_HOST_API_LIB_NAME}
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    NO_DEFAULT_PATH
)

# ==========================================
# 4. 查找 ROS 2 Jazzy 依赖包 (ament)
# ==========================================
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
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
find_package(ament_index_cpp REQUIRED) 

# ==========================================
# 5. 设置包含目录与公共链接库
# ==========================================
include_directories(
    include
    ${EIGEN3_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${PCL_INCLUDE_DIRS}
    ${LIBUSB_INCLUDE_DIRS}
    ${cv_bridge_INCLUDE_DIRS} 
)

# 直接将 yaml-cpp 目标加入公共库，解决所有 YAML 链接错误
set(COMMON_LIBS
    ${OpenCV_LIBS}
    ${PCL_LIBRARIES}
    yaml-cpp           
    ${OPENSSL_LIBRARIES}
    ${LIBUSB_LIBRARIES}
    ${LYD_HOST_API_LIB}
    pthread rt ${CMAKE_DL_LIBS}
)

# ==========================================
# 6. 定义编译目标 (严格匹配 ROS 2 源码)
# ==========================================

# 目标 1: host_sdk_sample
add_executable(host_sdk_sample 
    src/host_sdk_sample.cpp 
    src/yaml_parser.cpp 
    src/rawCloudRender.cpp 
    src/camera_pose_visualization.cpp
)
target_link_libraries(host_sdk_sample ${COMMON_LIBS} usb-1.0)
ament_target_dependencies(host_sdk_sample 
    rclcpp std_msgs sensor_msgs nav_msgs visualization_msgs 
    cv_bridge image_transport tf2 tf2_ros geometry_msgs tf2_geometry_msgs
    ament_index_cpp
)

# 目标 2: pointcloud_depth_converter_ros2 (库)
add_library(pointcloud_depth_converter_ros2 src/pointcloud_depth_converter.cpp)
target_link_libraries(pointcloud_depth_converter_ros2 ${OpenCV_LIBS} ${PCL_LIBRARIES})

# 目标 3: depth_image_ros2_node_lib (库)
add_library(depth_image_ros2_node_lib src/depth_image_ros2_node.cpp)
target_link_libraries(depth_image_ros2_node_lib pointcloud_depth_converter_ros2 ${OpenCV_LIBS} ${PCL_LIBRARIES})
ament_target_dependencies(depth_image_ros2_node_lib 
    rclcpp sensor_msgs std_msgs cv_bridge image_transport pcl_conversions message_filters
)

# 目标 4: pcd2depth_ros2_node 
add_executable(pcd2depth_ros2_node src/pcd2depth_ros2.cpp)
target_link_libraries(pcd2depth_ros2_node depth_image_ros2_node_lib ${OpenCV_LIBS} ${PCL_LIBRARIES} yaml-cpp)
ament_target_dependencies(pcd2depth_ros2_node 
    rclcpp sensor_msgs std_msgs cv_bridge image_transport pcl_conversions message_filters ament_index_cpp
)

# 目标 5: cloud_reprojector_ros2 (库)
add_library(cloud_reprojector_ros2 src/cloud_reprojector.cpp)
target_link_libraries(cloud_reprojector_ros2 ${OpenCV_LIBS} ${PCL_LIBRARIES})

# 目标 6: cloud_reprojection_ros2_node
add_executable(cloud_reprojection_ros2_node src/cloud_reprojection_ros.cpp)
target_link_libraries(cloud_reprojection_ros2_node cloud_reprojector_ros2 ${OpenCV_LIBS} ${PCL_LIBRARIES} yaml-cpp)
ament_target_dependencies(cloud_reprojection_ros2_node 
    rclcpp sensor_msgs nav_msgs cv_bridge image_transport pcl_conversions message_filters ament_index_cpp
)

# 目标 7: image_overlay_node
add_executable(image_overlay_node src/image_overlay_node.cpp)
target_link_libraries(image_overlay_node ${OpenCV_LIBS})
ament_target_dependencies(image_overlay_node 
    rclcpp sensor_msgs cv_bridge image_transport message_filters ament_index_cpp
)

# 目标 8: pcd_map_publisher
add_executable(pcd_map_publisher src/pcd_map_publisher.cpp)
target_link_libraries(pcd_map_publisher ${COMMON_LIBS} yaml-cpp)
ament_target_dependencies(pcd_map_publisher 
    rclcpp sensor_msgs pcl_conversions ament_index_cpp
)

# 目标 9: pose_listener (新增的提取真实坐标节点)
add_executable(pose_listener src/pose_listener.cpp)
ament_target_dependencies(pose_listener 
    rclcpp 
    tf2 
    tf2_ros 
    geometry_msgs
)

# ==========================================
# 7. 安装规则 (Install)
# ==========================================
install(TARGETS 
    host_sdk_sample 
    pcd2depth_ros2_node
    cloud_reprojection_ros2_node
    image_overlay_node
    pcd_map_publisher
    pose_listener          # <--- 安装新节点
    DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/ DESTINATION include)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/launch_ROS2")
    install(DIRECTORY launch_ROS2/ DESTINATION share/${PROJECT_NAME}/launch)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config")
    install(DIRECTORY config/ DESTINATION share/${PROJECT_NAME}/config)
endif()

# ==========================================
# 8. 导出依赖与打包
# ==========================================
ament_export_dependencies(
    rclcpp std_msgs sensor_msgs nav_msgs visualization_msgs 
    cv_bridge image_transport pcl_conversions message_filters
    tf2 tf2_ros geometry_msgs tf2_geometry_msgs ament_index_cpp
)

ament_package()