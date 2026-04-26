/*
PCD Map Publisher Node
Publishes a PCD file as PointCloud2 for visualization in rviz during relocalization mode.
*/

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl_conversions/pcl_conversions.h>
#include <string>
#include <filesystem>

class PcdMapPublisher : public rclcpp::Node
{
public:
    PcdMapPublisher()
        : Node("pcd_map_publisher")
    {
        this->declare_parameter<std::string>("pcd_file_path", "");
        this->declare_parameter<std::string>("topic_name", "/odin1/map_pcd");
        this->declare_parameter<std::string>("frame_id", "odom");
        this->declare_parameter<bool>("publish_once", true);
        this->declare_parameter<double>("publish_rate", 1.0);

        std::string pcd_file_path = this->get_parameter("pcd_file_path").as_string();
        std::string topic_name = this->get_parameter("topic_name").as_string();
        std::string frame_id = this->get_parameter("frame_id").as_string();
        publish_once_ = this->get_parameter("publish_once").as_bool();
        double publish_rate = this->get_parameter("publish_rate").as_double();

        publisher_ = this->create_publisher<sensor_msgs::msg::PointCloud2>(topic_name, 1);

        if (pcd_file_path.empty()) {
            RCLCPP_WARN(this->get_logger(), "pcd_file_path is empty, no map will be published");
            return;
        }

        if (!std::filesystem::exists(pcd_file_path)) {
            RCLCPP_ERROR(this->get_logger(), "PCD file not found: %s", pcd_file_path.c_str());
            return;
        }

        try {
            cloud_ = std::make_shared<pcl::PointCloud<pcl::PointXYZRGBA>>();
            if (pcl::io::loadPCDFile<pcl::PointXYZRGBA>(pcd_file_path, *cloud_) == -1) {
                RCLCPP_ERROR(this->get_logger(), "Failed to load PCD file: %s", pcd_file_path.c_str());
                return;
            }

            cloud_->header.frame_id = frame_id;
            RCLCPP_INFO(this->get_logger(), "Loaded PCD with %zu points from %s",
                       cloud_->size(), pcd_file_path.c_str());

            if (publish_once_) {
                publishMap();
                RCLCPP_INFO(this->get_logger(), "Map published once, node will continue running");
            } else {
                auto period = std::chrono::duration<double>(1.0 / publish_rate);
                timer_ = this->create_wall_timer(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::duration<double>(period)),
                    [this]() { this->publishMap(); });
            }
        } catch (const std::exception& e) {
            RCLCPP_ERROR(this->get_logger(), "Exception loading PCD: %s", e.what());
        }
    }

private:
    void publishMap()
    {
        if (!cloud_ || publisher_->get_subscription_count() == 0) {
            return;
        }

        sensor_msgs::msg::PointCloud2 output;
        pcl::toROSMsg(*cloud_, output);
        output.header.stamp = this->now();
        output.header.frame_id = cloud_->header.frame_id;
        publisher_->publish(output);
    }

    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
    pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud_;
    bool publish_once_;
};

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<PcdMapPublisher>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
