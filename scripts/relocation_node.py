#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
import tf2_ros
import numpy as np
from scipy.spatial.transform import Rotation as R

# ==========================================
# 粘贴你刚刚写的类 (完全保留原汁原味)
# ==========================================
class PoseTransformerQuat:
    def __init__(self):
        pass

    def _pose_to_matrix(self, x, y, z, qx, qy, qz, qw):
        T = np.eye(4)
        T[:3, :3] = R.from_quat([qx, qy, qz, qw]).as_matrix()
        T[:3, 3] = [x, y, z]
        return T

    def _matrix_to_pose(self, T):
        x, y, z = T[:3, 3]
        quat = R.from_matrix(T[:3, :3]).as_quat()
        return x, y, z, quat[0], quat[1], quat[2], quat[3]

    def apply_matrix_to_pose(self, current_pose, transform_matrix):
        T_current = self._pose_to_matrix(*current_pose)
        T_new = transform_matrix @ T_current
        new_pose = self._matrix_to_pose(T_new)
        return new_pose
# ==========================================

class RelocationNode(Node):
    def __init__(self):
        super().__init__('relocation_node')
        # 初始化 TF 监听器
        self.tf_buffer = tf2_ros.Buffer()
        self.tf_listener = tf2_ros.TransformListener(self.tf_buffer, self)
        
        # 初始化你的计算工具
        self.transformer = PoseTransformerQuat()

        # 发布者和订阅者
        self.publisher = self.create_publisher(Odometry, '/odin1/relocation', 10)
        self.subscription = self.create_subscription(Odometry, '/odin1/odometry', self.odom_callback, 10)
        
        self.get_logger().info("Python 版本的重定位节点已启动！(基于自定义矩阵算法)")

    def odom_callback(self, msg: Odometry):
        try:
            # 1. 自动查询 ROS 2 TF 树中的 4x4 变换关系 (map -> odom)
            trans = self.tf_buffer.lookup_transform('map', msg.header.frame_id, rclpy.time.Time())
            
            # 2. 将 TF 结果转成你的工具需要的 4x4 NumPy 矩阵
            tx, ty, tz = trans.transform.translation.x, trans.transform.translation.y, trans.transform.translation.z
            rx, ry, rz, rw = trans.transform.rotation.x, trans.transform.rotation.y, trans.transform.rotation.z, trans.transform.rotation.w
            
            tf_matrix = np.eye(4)
            tf_matrix[:3, :3] = R.from_quat([rx, ry, rz, rw]).as_matrix()
            tf_matrix[:3, 3] = [tx, ty, tz]

            # 3. 提取当前 Odometry 的位置和姿态
            p = msg.pose.pose.position
            q = msg.pose.pose.orientation
            current_pose = (p.x, p.y, p.z, q.x, q.y, q.z, q.w)

            # 4. 调用你的核心算法进行变换！
            new_pose = self.transformer.apply_matrix_to_pose(current_pose, tf_matrix)

            # 5. 打包并发布新的坐标
            relocated_odom = Odometry()
            relocated_odom.header.stamp = msg.header.stamp
            relocated_odom.header.frame_id = 'map'
            relocated_odom.child_frame_id = msg.child_frame_id
            relocated_odom.twist = msg.twist # 保留原有的速度信息

            relocated_odom.pose.pose.position.x = new_pose[0]
            relocated_odom.pose.pose.position.y = new_pose[1]
            relocated_odom.pose.pose.position.z = new_pose[2]
            relocated_odom.pose.pose.orientation.x = new_pose[3]
            relocated_odom.pose.pose.orientation.y = new_pose[4]
            relocated_odom.pose.pose.orientation.z = new_pose[5]
            relocated_odom.pose.pose.orientation.w = new_pose[6]
            relocated_odom.pose.covariance = msg.pose.covariance

            self.publisher.publish(relocated_odom)

        except tf2_ros.TransformException as ex:
            # 限制打印频率，避免刷屏
            pass 

def main(args=None):
    rclpy.init(args=args)
    node = RelocationNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()