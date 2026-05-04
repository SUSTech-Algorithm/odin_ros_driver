#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>

class PcdPublisherNode : public rclcpp::Node {
public:
    PcdPublisherNode() : Node("pcd_publisher_node") {
        // 1. 声明并获取参数
        this->declare_parameter<std::string>("pcd_file_path", "");
        this->declare_parameter<std::string>("topic_name", "/odin1/map");
        this->declare_parameter<std::string>("frame_id", "map");
        this->declare_parameter<int>("publish_interval_ms", 1000);

        std::string pcd_path = this->get_parameter("pcd_file_path").as_string();
        std::string topic_name = this->get_parameter("topic_name").as_string();
        frame_id_ = this->get_parameter("frame_id").as_string();
        int interval_ms = this->get_parameter("publish_interval_ms").as_int();

        // 2. 设置 QoS (transient_local 保证后加入的节点也能收到静态地图)
        rclcpp::QoS map_qos(rclcpp::KeepLast(1));
        map_qos.transient_local();
        map_pub_ = this->create_publisher<sensor_msgs::msg::PointCloud2>(topic_name, map_qos);

        // 3. 加载 PCD 文件，如果失败则直接停止节点后续工作
        if (!load_pcd_file(pcd_path)) {
            RCLCPP_ERROR(this->get_logger(), "!!! 节点启动失败：地图加载中止 !!!");
            return;
        }

        // 4. 创建定时器，循环发布
        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(interval_ms),
            std::bind(&PcdPublisherNode::timer_callback, this));
            
        RCLCPP_INFO(this->get_logger(), "✅ PCD地图加载成功！正在发布至话题: %s", topic_name.c_str());
    }

private:
    bool load_pcd_file(const std::string& path) {
        // 检查路径是否为空 (通常是因为 param.yaml 没挂载上)
        if (path.empty()) {
            RCLCPP_ERROR(this->get_logger(), "PCD 路径为空！请检查 launch 文件是否正确加载了 param.yaml！");
            return false;
        }

        pcl::PointCloud<pcl::PointXYZI> cloud; 
        
        // 尝试读取文件
        if (pcl::io::loadPCDFile<pcl::PointXYZI>(path, cloud) == -1) {
            RCLCPP_ERROR(this->get_logger(), "无法读取 PCD 文件，路径错误或文件不存在: %s", path.c_str());
            RCLCPP_ERROR(this->get_logger(), "-> 请检查该绝对路径下是否真的有这个 .pcd 文件！");
            return false;
        }

        // 将 PCL 点云转换为 ROS 2 消息格式
        pcl::toROSMsg(cloud, cloud_msg_);
        cloud_msg_.header.frame_id = frame_id_;
        return true;
    }

    void timer_callback() {
        // 每次发布时更新时间戳
        cloud_msg_.header.stamp = this->get_clock()->now();
        map_pub_->publish(cloud_msg_);
    }

    std::string frame_id_;
    sensor_msgs::msg::PointCloud2 cloud_msg_;
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr map_pub_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<PcdPublisherNode>());
    rclcpp::shutdown();
    return 0;
}