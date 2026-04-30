#include <rclcpp/rclcpp.hpp>
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/buffer.h>
#include <geometry_msgs/msg/transform_stamped.hpp>
// ================= 新增 =================
// 引入标准的位置姿态消息头文件
#include <geometry_msgs/msg/pose_stamped.hpp> 
// ========================================

class PoseListenerNode : public rclcpp::Node
{
public:
    PoseListenerNode() : Node("pose_listener_node")
    {
        tf_buffer_ = std::make_unique<tf2_ros::Buffer>(this->get_clock());
        tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

        // ================= 新增 =================
        // 创建一个发布者，话题名叫 "/odin1/real_pose" (你可以随便改)，队列长度设为 10
        pose_pub_ = this->create_publisher<geometry_msgs::msg::PoseStamped>("/odin1/real_pose", 10);
        // ========================================

        // 定时器：我现在改成了 100 毫秒（10Hz）。
        // 既然要对外发布数据，1秒钟发1次太慢了，10Hz 是比较适合追踪轨迹的速度。
        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(100), 
            std::bind(&PoseListenerNode::on_timer, this));
            
        RCLCPP_INFO(this->get_logger(), "坐标提取节点已启动！将把真实坐标发布到话题: /odin1/real_pose");
    }

private:
    void on_timer()
    {
        try {
            // 1. 向系统查询真实坐标
            geometry_msgs::msg::TransformStamped t = tf_buffer_->lookupTransform(
                "odom",          // 你的起点名称
                "odin_link",     // 你的雷达名称
                tf2::TimePointZero
            );

            // ================= 新增 =================
            // 2. 创建一个准备发出去的消息盒子
            geometry_msgs::msg::PoseStamped pose_msg;
            
            // 3. 填入时间戳和坐标系名称
            pose_msg.header.stamp = this->get_clock()->now();
            pose_msg.header.frame_id = "odom"; // 必须明确告诉别人，这个坐标是基于 odom 算的

            // 4. 填入 XYZ 空间位置
            pose_msg.pose.position.x = t.transform.translation.x;
            pose_msg.pose.position.y = t.transform.translation.y;
            pose_msg.pose.position.z = t.transform.translation.z;

            // 5. 填入旋转姿态 (四元数)
            pose_msg.pose.orientation = t.transform.rotation;

            // 6. 广播出去！
            pose_pub_->publish(pose_msg);
            // ========================================

            // 为了不让终端疯狂刷屏，用 INFO_THROTTLE 限制每 1 秒在终端打印一次即可，但实际后台是 10Hz 在发布的
            RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
                "当前坐标 -> X: %.2f, Y: %.2f, Z: %.2f", 
                pose_msg.pose.position.x, pose_msg.pose.position.y, pose_msg.pose.position.z);

        } catch (const tf2::TransformException & ex) {
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000, 
                "等待坐标数据对齐... : %s", ex.what());
        }
    }

    std::unique_ptr<tf2_ros::Buffer> tf_buffer_;
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
    rclcpp::TimerBase::SharedPtr timer_;
    
    // ================= 新增 =================
    // 声明发布者的指针
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr pose_pub_;
    // ========================================
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<PoseListenerNode>());
    rclcpp::shutdown();
    return 0;
}



