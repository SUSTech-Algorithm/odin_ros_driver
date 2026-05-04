#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import PointCloud2
import sensor_msgs_py.point_cloud2 as pc2
from std_msgs.msg import Header
import numpy as np
import open3d as o3d

class PcdPublisherNode(Node):
    def __init__(self):
        super().__init__('pcd_publisher_node')
        
        # 1. 声明并获取参数
        self.declare_parameter('pcd_file_path', '')
        self.declare_parameter('topic_name', '/odin1/map')
        self.declare_parameter('frame_id', 'map')
        self.declare_parameter('publish_interval_ms', 1000)

        pcd_path = self.get_parameter('pcd_file_path').value
        topic_name = self.get_parameter('topic_name').value
        self.frame_id = self.get_parameter('frame_id').value
        interval_ms = self.get_parameter('publish_interval_ms').value

        # 2. 设置 QoS (Transient Local，专为静态地图和后加入的 RViz 节点设计)
        from rclpy.qos import QoSProfile, DurabilityPolicy
        qos_profile = QoSProfile(depth=1)
        qos_profile.durability = DurabilityPolicy.TRANSIENT_LOCAL

        self.map_pub = self.create_publisher(PointCloud2, topic_name, qos_profile)
        self.cloud_msg = None # 用来缓存建好的点云消息

        # 3. 加载并转换 PCD 文件（只在这里执行一次耗时操作！）
        if not self.load_pcd_file(pcd_path):
            self.get_logger().error("!!! 节点启动失败：地图加载中止 !!!")
            return

        # 4. 启动轻量级定时器，仅负责发送
        interval_sec = interval_ms / 1000.0
        self.timer = self.create_timer(interval_sec, self.timer_callback)
        self.get_logger().info(f"✅ PCD地图加载并转换成功！正在轻松发布至话题: {topic_name}")

    def load_pcd_file(self, path):
        if not path:
            self.get_logger().error("PCD 路径为空！请检查 param.yaml 文件是否正确挂载。")
            return False
            
        try:
            self.get_logger().info(f"正在读取 PCD 文件: {path} ...")
            pcd = o3d.io.read_point_cloud(path)
            if not pcd.has_points():
                self.get_logger().error("无法读取 PCD 文件，或文件内容为空！请检查绝对路径。")
                return False
                
            points = np.asarray(pcd.points)
            self.get_logger().info(f"成功读取 {len(points)} 个点！正在打包为 ROS 2 格式 (只需执行一次，请稍候)...")
            
            # 核心性能优化：在节点启动时，一次性将 numpy 数组转换为完整的 ROS 2 二进制消息
            header = Header()
            header.frame_id = self.frame_id
            self.cloud_msg = pc2.create_cloud_xyz32(header, points.tolist())
            
            return True
        except Exception as e:
            self.get_logger().error(f"读取 PCD 失败: {e}")
            return False

    def timer_callback(self):
        # 定时器里只做极轻量级的操作：更新时间戳并直接把缓存的二进制消息发出去
        if self.cloud_msg is not None:
            self.cloud_msg.header.stamp = self.get_clock().now().to_msg()
            self.map_pub.publish(self.cloud_msg)

def main(args=None):
    rclpy.init(args=args)
    node = PcdPublisherNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()