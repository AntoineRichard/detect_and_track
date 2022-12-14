#include <string>
#include <vector>
#include <map>

#include <detect_and_track/ObjectDetection.h>
#include <detect_and_track/PoseEstimator.h>
#include <detect_and_track/BoundingBox2D.h>
#include <detect_and_track/BoundingBoxes2D.h>
#include <detect_and_track/PositionBoundingBox2D.h>
#include <detect_and_track/PositionBoundingBox2DArray.h>
#include <detect_and_track/PositionID.h>
#include <detect_and_track/PositionIDArray.h>

// ROS
#include <opencv2/opencv.hpp>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/CameraInfo.h>
#include <image_transport/image_transport.h>
#include <tf/transform_listener.h>
#include <ros/ros.h>

class ROSDetector {
  private:
    ros::NodeHandle nh_;
    image_transport::ImageTransport it_;

    image_transport::Subscriber image_sub_;
    image_transport::Subscriber depth_sub_;

#ifdef PUBLISH_DETECTION_IMAGE   
    image_transport::Publisher detection_pub_;
#endif
    ros::Subscriber depth_info_sub_;
#ifdef PUBLISH_DETECTION_WITH_POSITION
    ros::Publisher positions_bboxes_pub_;
#else
    ros::Publisher bboxes_pub_;
    ros::Publisher positions_pub_;
#endif

    // Image parameters
    int image_size_;
    int image_rows_;
    int image_cols_;
    int padding_rows_;
    int padding_cols_;
    cv::Mat padded_image_;
    cv::Mat depth_image_;
    sensor_msgs::ImagePtr image_ptr_out_;
    
    // Object detector parameters
    int num_classes_;
    std::vector<std::string> class_map_;

    // Transform parameters
    tf::TransformListener listener_;
    std::vector<std::string> object_frames_;
    std::string global_frame_;

    ObjectDetector* OD_;
    PoseEstimator* PE_;

    void imageCallback(const sensor_msgs::ImageConstPtr&);
    void depthInfoCallback(const sensor_msgs::CameraInfoConstPtr&);
    void depthCallback(const sensor_msgs::ImageConstPtr&);
    void adjustBoundingBoxes(std::vector<std::vector<BoundingBox>>&);
    void padImage(const cv::Mat&);

  public:
    ROSDetector();
    ~ROSDetector();
};

ROSDetector::ROSDetector() : nh_("~"), it_(nh_), OD_(), PE_() { 
  // Object detector parameters
  std::vector<std::string> default_class_map {std::string("object")};
  std::string default_path_to_engine("None");
  std::string path_to_engine;
  nh_.param("nms_tresh",nms_tresh,0.45f);
  nh_.param("image_size",image_size_,640);
  nh_.param("conf_tresh",conf_tresh,0.25f);
  nh_.param("max_output_bbox_count", max_output_bbox_count, 1000);
  nh_.param("path_to_engine", path_to_engine, default_path_to_engine);
  nh_.param("num_classes", num_classes_, 1);
  nh_.param("class_map", class_map_, default_class_map);
  nh_.param("image_rows", image_rows_, 480);
  nh_.param("image_cols", image_cols_, 640);

  // Tracker parameters
  float nms_tresh, conf_tresh;
  int max_output_bbox_count;
  std::vector<float> default_Q {9.0, 9.0, 200.0, 200.0, 5.0, 5.0};
  std::vector<float> default_R {2.0, 2.0, 200.0, 200.0, 2.0, 2.0};
  Q_.resize(6);
  R_.resize(6);
  nh_.param("max_frames_to_skip", max_frames_to_skip_, 15);
  nh_.param("dist_threshold", dist_threshold_, 150.0f);
  nh_.param("center_threshold", center_threshold_, 80.0f);
  nh_.param("area_threshold", area_threshold_, 3.0f);
  nh_.param("body_ratio", body_ratio_, 0.5f);
  nh_.param("dt", dt_, 0.02f);
  nh_.param("use_dim", use_dim_, true);
  nh_.param("use_vel", use_vel_, false);
  nh_.param("Q", Q_, default_Q);
  nh_.param("R", R_, default_R);
  nh_.param("max_bbox_width", max_bbox_width_, 400.0f);
  nh_.param("max_bbox_height", max_bbox_height_, 300.0f);
  nh_.param("min_bbox_width", min_bbox_width_, 60.0f);
  nh_.param("min_bbox_height", min_bbox_height_, 60.0f);

  OD_ = new ObjectDetector(path_to_engine, nms_tresh, conf_tresh,max_output_bbox_count, 2, image_size_, num_classes_);
  PE_ = new PoseEstimator(0.02, 0.15, 58.0, 87.0, image_rows_, image_cols_);
  TR_ = new Tracker(max_frames_to_skip_, dist_threshold_, center_threshold_,
                    area_threshold_, body_ratio_, dt_, use_dim_,
                    use_z_, use_vel_, Q_, R_);
  
  padded_image_ = cv::Mat::zeros(image_size_, image_size_, CV_8UC3);

  image_sub_ = it_.subscribe("/camera/color/image_raw", 1, &ROSDetector::imageCallback, this);
  depth_sub_ = it_.subscribe("/camera/aligned_depth_to_color/image_raw", 1, &ROSDetector::depthCallback, this);
  depth_info_sub_ = nh_.subscribe("/camera/aligned_depth_to_color/camera_info", 1, &ROSDetector::depthInfoCallback, this);
#ifdef PUBLISH_DETECTION_IMAGE
  detection_pub_ = it_.advertise("/detection/raw_detection", 1);
#endif
#ifdef PUBLISH_DETECTION_WITH_POSITION
  positions_bboxes_pub_ = nh_.advertise<detect_and_track::PositionBoundingBox2DArray>("/detection/positions_bboxes",1);
#else
  positions_pub_ = nh_.advertise<detect_and_track::PositionIDArray>("/detection/positions",1);
  bboxes_pub_ = nh_.advertise<detect_and_track::BoundingBox2D>("/detection/bounding_boxes", 1);
#endif
}

ROSDetector::~ROSDetector() {
}

void ROSDetector::depthInfoCallback(const sensor_msgs::CameraInfoConstPtr& msg){
  PE_->updateCameraParameters(msg->K[2], msg->K[5], msg->K[0], msg->K[4], msg->D);
}

void ROSDetector::padImage(const cv::Mat& image) {
  float r;
  r = (float) image_size_ / std::max(image.rows, image.cols);
  ROS_INFO("%f",r);
  if (r != 1) {
    ROS_ERROR("Not implemented");
  } else {
    padding_rows_ = (image_size_ - image.rows)/2;
    padding_cols_ = (image_size_ - image.cols)/2;
    image.copyTo(padded_image_(cv::Range(padding_rows_,padding_rows_+image.rows),cv::Range(padding_cols_,padding_cols_+image.cols)));
  }
}

void ROSDetector::adjustBoundingBoxes(std::vector<std::vector<BoundingBox>>& bboxes) {
  for (unsigned int i=0; i < bboxes.size(); i++) {
    for (unsigned int j=0; j < bboxes[i].size(); j++) {
      if (!bboxes[i][j].valid_) {
        continue;
      }
      bboxes[i][j].x_ -= padding_cols_;
      bboxes[i][j].y_ -= padding_rows_;
      bboxes[i][j].x_min_ = std::max(bboxes[i][j].x_ - bboxes[i][j].w_/2, (float) 0.0);
      bboxes[i][j].x_max_ = std::min(bboxes[i][j].x_ + bboxes[i][j].w_/2, (float) image_cols_);
      bboxes[i][j].y_min_ = std::max(bboxes[i][j].y_ - bboxes[i][j].h_/2, (float) 0.0);
      bboxes[i][j].y_max_ = std::min(bboxes[i][j].y_ + bboxes[i][j].h_/2, (float) image_rows_);
    }
  }
}

void ROSDetector::depthCallback(const sensor_msgs::ImageConstPtr& msg){
  cv_bridge::CvImagePtr cv_ptr;
  try {
    cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::TYPE_16UC1);
  }
  catch (cv_bridge::Exception& e) {
    ROS_ERROR("cv_bridge exception: %s", e.what());
    return;
  }
  cv_ptr->image.convertTo(depth_image_, CV_32F, 0.001);
}


void ROSDetector::imageCallback(const sensor_msgs::ImageConstPtr& msg){
#ifdef PROFILE
  auto start_image = std::chrono::system_clock::now();
#endif
  cv_bridge::CvImagePtr cv_ptr;
  try {
    cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
  } catch (cv_bridge::Exception &e) {
    ROS_ERROR("cv_bridge exception: %s", e.what());
    return;
  }
  cv::Mat image = cv_ptr->image;
  cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
  cv::Mat image_tracker = image.clone();
  padImage(image);
#ifdef PROFILE
  auto end_image = std::chrono::system_clock::now();
  auto start_detection = std::chrono::system_clock::now();
#endif
  std::vector<std::vector<BoundingBox>> bboxes(ObjectClass::NUM_CLASS);
  OD_->detectObjects(padded_image_, bboxes);
  adjustBoundingBoxes(bboxes);
#ifdef PROFILE
  auto end_detection = std::chrono::system_clock::now();
  auto start_distance = std::chrono::system_clock::now();
#endif
  std::vector<std::vector<float>> distances;
  distances = PE_->extractDistanceFromDepth(depth_image_, bboxes);
#ifdef PROFILE
  auto end_distance = std::chrono::system_clock::now();
  auto start_position = std::chrono::system_clock::now();
#endif
  std::vector<std::vector<std::vector<float>>> points;
  points = PE_->estimatePosition(distances, bboxes);
#ifdef PROFILE
  auto end_position = std::chrono::system_clock::now();
  ROS_INFO("Full inference done in %ld ms", std::chrono::duration_cast<std::chrono::milliseconds>(end_position - start_image).count());
  ROS_INFO(" - Image processing done in %ld us", std::chrono::duration_cast<std::chrono::microseconds>(end_image - start_image).count());
  ROS_INFO(" - Object detection done in %ld ms", std::chrono::duration_cast<std::chrono::milliseconds>(end_detection - start_detection).count());
  ROS_INFO(" - Distance estimation done in %ld us", std::chrono::duration_cast<std::chrono::microseconds>(end_distance - start_distance).count());
  ROS_INFO(" - Position estimation done in %ld us", std::chrono::duration_cast<std::chrono::microseconds>(end_position - start_position).count());
#endif

#ifdef PUBLISH_DETECTION_IMAGE   
  for (unsigned int i=0; i<bboxes.size(); i++) {
    for (unsigned int j=0; j<bboxes[i].size(); j++) {
      if (!bboxes[i][j].valid_) {
        continue;
      }
      const cv::Rect rect(bboxes[i][j].x_min_, bboxes[i][j].y_min_, bboxes[i][j].w_, bboxes[i][j].h_);
      cv::rectangle(image, rect, ColorPalette[i], 3);
      cv::putText(image, ClassMap[i], cv::Point(bboxes[i][j].x_min_,bboxes[i][j].y_min_-10), cv::FONT_HERSHEY_SIMPLEX, 0.9, ColorPalette[i], 2);
    }
  }

  cv::cvtColor(image, image, cv::COLOR_RGB2BGR);
  std_msgs::Header image_ptr_out_header;
  image_ptr_out_header.stamp = ros::Time::now();
  image_ptr_out_ = cv_bridge::CvImage(image_ptr_out_header, "bgr8", image).toImageMsg();
  detection_pub_.publish(image_ptr_out_);
#endif
#ifdef PUBLISH_DETECTION_WITH_POSITION
  detect_and_track::PositionBoundingBox2DArray ros_bboxes;
  detect_and_track::PositionBoundingBox2D ros_bbox;
  std::vector<detect_and_track::PositionBoundingBox2D> vec_ros_bboxes;
  for (unsigned int i=0; i<bboxes.size(); i++) {
    for (unsigned int j=0; j<bboxes[i].size(); j++) {
      if (!bboxes[i][j].valid_) {
        continue;
      }
      ros_bbox.bbox.min_x = bboxes[i][j].x_min_;
      ros_bbox.bbox.min_y = bboxes[i][j].y_min_;
      ros_bbox.bbox.height = bboxes[i][j].h_;
      ros_bbox.bbox.width = bboxes[i][j].w_;
      ros_bbox.bbox.class_id = i;
      ros_bbox.position.x = points[i][j][0];
      ros_bbox.position.y = points[i][j][1];
      ros_bbox.position.z = points[i][j][2];
      vec_ros_bboxes.push_back(ros_bbox);
    }
  }
  ros_bboxes.header.stamp = cv_ptr->header.stamp;
  ros_bboxes.header.frame_id = cv_ptr->header.frame_id;
  ros_bboxes.bboxes = vec_ros_bboxes;
  
  positions_bboxes_pub_.publish(ros_bboxes);

#else
  unsigned int counter = 0;
  detect_and_track::BoundingBoxes2D ros_bboxes;
  detect_and_track::BoundingBox2D ros_bbox;
  detect_and_track::PositionIDArray id_positions;
  detect_and_track::PositionID id_position;
  std::vector<detect_and_track::BoundingBox2D> vec_ros_bboxes;
  std::vector<detect_and_track::PositionID> vec_id_positions;

  for (unsigned int i=0; i<bboxes.size(); i++) {
    for (unsigned int j=0; j<bboxes[i].size(); j++) {
      if (!bboxes[i][j].valid_) {
        continue;
      }
      ros_bbox.min_x = bboxes[i][j].x_min_;
      ros_bbox.min_y = bboxes[i][j].y_min_;
      ros_bbox.height = bboxes[i][j].h_;
      ros_bbox.width = bboxes[i][j].w_;
      ros_bbox.class_id = i;
      ros_bbox.detection_id = counter;
      id_position.position.x = points[i][j][0];
      id_position.position.y = points[i][j][1];
      id_position.position.z = points[i][j][2];
      id_position.detection_id = counter;
      counter ++;
      vec_ros_bboxes.push_back(ros_bbox);
      vec_id_positions.push_back(id_position);
    }
  }
  ros_bboxes.header.stamp = cv_ptr->header.stamp;
  ros_bboxes.header.frame_id = cv_ptr->header.frame_id;
  ros_bboxes.bboxes = vec_ros_bboxes;
  id_positions.header.stamp = cv_ptr->header.stamp;
  id_positions.header.frame_id = cv_ptr->header.frame_id;
  id_positions.positions = vec_id_positions;

  bboxes_pub_.publish(ros_bboxes);
  positions_pub_.publish(id_positions);
#endif
}

int main(int argc, char** argv)
{
  ros::init(argc, argv, "drone_detector");
  ROSDetector rd;
  ros::spin();
  return 0;
}