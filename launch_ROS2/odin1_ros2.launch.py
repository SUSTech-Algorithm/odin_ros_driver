import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    # 获取 odin_ros_driver 包的安装路径
    pkg_dir = get_package_share_directory('odin_ros_driver')
    
    # 定义配置文件路径
    # 驱动自带的配置文件
    control_param_file = os.path.join(pkg_dir, 'config', 'control_command.yaml')
    # 我们自己写的 Task 任务配置文件
    task_param_file = os.path.join(pkg_dir, 'config', 'param.yaml')
    # RViz 配置文件 (如果有的话)
    rviz_config_file = os.path.join(pkg_dir, 'config', 'rviz2', 'odin_rviz2.rviz') 

    return LaunchDescription([
        # 1. 雷达核心驱动节点 (C++)
        Node(
            package='odin_ros_driver',
            executable='host_sdk_sample',
            name='host_sdk_sample',
            output='screen',
            parameters=[control_param_file]
        ),
        
        # 2. 深度图转换节点 (C++)
        Node(
            package='odin_ros_driver',
            executable='pcd2depth_ros2_node',
            name='pcd2depth_ros2_node',
            output='screen',
            parameters=[control_param_file]
        ),
        
        # 3. 点云重投影节点 (C++)
        Node(
            package='odin_ros_driver',
            executable='cloud_reprojection_ros2_node',
            name='cloud_reprojection_ros2_node',
            output='screen',
            parameters=[control_param_file]
        ),
        
        # 4. 图像叠加节点 (C++)
        Node(
            package='odin_ros_driver',
            executable='image_overlay_node',
            name='image_overlay_node',
            output='screen',
        ),

        # ==========================================
        # 新增: Task 1 - 四元数重定位节点 (Python)
        # ==========================================
        Node(
            package='odin_ros_driver',
            executable='relocation_node.py',
            name='relocation_node',
            output='screen',
        ),

        # ==========================================
        # 新增: Task 2 - 离线 PCD 地图发布节点 (Python)
        # (重点：这里已经改成了 pcd_publisher_node.py)
        # ==========================================
        Node(
            package='odin_ros_driver',
            executable='pcd_publisher_node.py',
            name='pcd_publisher_node',
            output='screen',
            parameters=[task_param_file]
        ),

        # 7. RViz2 可视化工具
        # 如果你之前没有特定的 rviz 配置文件，可以把 arguments 注释掉
        Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            output='screen',
            # arguments=['-d', rviz_config_file]  # 如果报错找不到rviz配置，可以把这行注释掉
        )
    ])