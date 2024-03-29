#include <detect_and_track/DetectionUtils.h>

Detect::Detect() : OD_() {}

Detect::Detect(GlobalParameters& global_parameters, DetectionParameters& detection_parameters,
               NMSParameters& nms_parameters) : OD_() {
  // Object detector parameters
  image_rows_ = global_parameters.image_height;
  image_cols_ = global_parameters.image_width;
  num_classes_ = detection_parameters.num_classes;
  class_map_ = detection_parameters.class_map;

  image_size_ = std::max(image_cols_, image_rows_);
  padded_image_ = cv::Mat::zeros(image_size_, image_size_, CV_8UC3);

  // Object instantiation
  OD_ = new ObjectDetector(image_size_, detection_parameters, nms_parameters);
}

void Detect::buildDetect(GlobalParameters& global_parameters, DetectionParameters& detection_parameters,
               NMSParameters& nms_parameters) {
  // Object detector parameters
  image_rows_ = global_parameters.image_height;
  image_cols_ = global_parameters.image_width;
  num_classes_ = detection_parameters.num_classes;
  class_map_ = detection_parameters.class_map;

  image_size_ = std::max(image_cols_, image_rows_);
  padded_image_ = cv::Mat::zeros(image_size_, image_size_, CV_8UC3);

  // Object instantiation
  OD_ = new ObjectDetector(image_size_, detection_parameters, nms_parameters);
}

Detect::~Detect() {}

void Detect::padImage(cv::Mat& image) {
  r_ = (float) image_size_ / std::max(image.rows, image.cols);
  cv::Mat tmp;
  printf("DEBUG: %f\n",r_);
  //if (r_ != 1) {
  cv::resize(image, tmp, cv::Size(), r_, r_, cv::INTER_AREA);
    //printf("Not implemented\n");
  //}
  //printf("rows %d, cols%d\n", image.rows, image.cols);
  padding_rows_ = (image_size_ - tmp.rows)/2;
  padding_cols_ = (image_size_ - tmp.cols)/2;
  tmp.copyTo(padded_image_(cv::Range(padding_rows_,padding_rows_+tmp.rows),cv::Range(padding_cols_,padding_cols_+tmp.cols)));  
}

void Detect::adjustBoundingBoxes(std::vector<std::vector<BoundingBox>>& bboxes) {
  for (unsigned int i=0; i < bboxes.size(); i++) {
    for (unsigned int j=0; j < bboxes[i].size(); j++) {
      if (!bboxes[i][j].valid_) {
        continue;
      }
      bboxes[i][j].x_ -= padding_cols_;
      bboxes[i][j].y_ -= padding_rows_;
      bboxes[i][j].x_ /= r_;
      bboxes[i][j].y_ /= r_;
      bboxes[i][j].x_min_ = bboxes[i][j].x_ - bboxes[i][j].w_/(2*r_);
      bboxes[i][j].x_max_ = bboxes[i][j].x_ + bboxes[i][j].w_/(2*r_);
      bboxes[i][j].y_min_ = bboxes[i][j].y_ - bboxes[i][j].h_/(2*r_);
      bboxes[i][j].y_max_ = bboxes[i][j].y_ + bboxes[i][j].h_/(2*r_);
      bboxes[i][j].h_ = bboxes[i][j].h_ / r_;
      bboxes[i][j].w_ = bboxes[i][j].w_ / r_;
      //bboxes[i][j].x_min_ = std::max(bboxes[i][j].x_ - bboxes[i][j].w_/(2*r_), (float) 0.0);
      //bboxes[i][j].x_max_ = std::min(bboxes[i][j].x_ + bboxes[i][j].w_/(2*r_), (float) image_cols_);
      //bboxes[i][j].y_min_ = std::max(bboxes[i][j].y_ - bboxes[i][j].h_/(2*r_), (float) 0.0);
      //bboxes[i][j].y_max_ = std::min(bboxes[i][j].y_ + bboxes[i][j].h_/(2*r_), (float) image_rows_);
    }
  }
}

void Detect::generateDetectionImage(cv::Mat& image, const std::vector<std::vector<BoundingBox>>& bboxes) {
  for (unsigned int i=0; i<bboxes.size(); i++) {
    for (unsigned int j=0; j<bboxes[i].size()+1; j++) {
      if (!bboxes[i][j].valid_) {
        continue;
      }
      const cv::Rect rect(bboxes[i][j].x_min_, bboxes[i][j].y_min_, bboxes[i][j].w_, bboxes[i][j].h_);
      cv::rectangle(image, rect, ColorPalette[0], 3);
      cv::putText(image, class_map_[i], cv::Point(bboxes[i][j].x_min_,bboxes[i][j].y_min_-10), cv::FONT_HERSHEY_SIMPLEX, 0.9, ColorPalette[i], 2);
    }
  }
}


void Detect::detectObjects(cv::Mat& image, std::vector<std::vector<BoundingBox>>& bboxes) {
  bboxes.clear();
  bboxes.resize(num_classes_);
#ifdef PROFILE
  start_image_ = std::chrono::system_clock::now();
#endif
  padImage(image);
#ifdef PROFILE
  end_image_ = std::chrono::system_clock::now();
  start_detection_ = std::chrono::system_clock::now();
#endif
  OD_->detectObjects(padded_image_, bboxes);
  adjustBoundingBoxes(bboxes);
#ifdef PROFILE
  end_detection_ = std::chrono::system_clock::now();
#endif
}

void Detect::printProfilingDetection() {
#ifdef PROFILE
  printf(" - Image processing done in %ld us\n", std::chrono::duration_cast<std::chrono::microseconds>(end_image_ - start_image_).count());
  printf(" - Object detection done in %ld ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(end_detection_ - start_detection_).count());
#endif
}

void Detect::applyOnFolder(std::string, std::string, bool, bool, bool) {}

void Detect::applyOnVideo(std::string, std::string, bool, bool, bool) {}

Locate::Locate() : PE_() {}

Locate::Locate(GlobalParameters& glo_p, LocalizationParameters& loc_p, CameraParameters& cam_p) : PE_() {
  // Object instantiation
  PE_ = new PoseEstimator(glo_p, loc_p, cam_p);
}

void Locate::buildLocate(GlobalParameters& glo_p, LocalizationParameters& loc_p, CameraParameters& cam_p) {
  // Object instantiation
  PE_ = new PoseEstimator(glo_p, loc_p, cam_p);
}

Locate::~Locate() {}

void Locate::updateCameraInfo(const std::vector<float>& camera_parameters, const std::vector<float>& lens_parameters){
  PE_->updateCameraParameters(camera_parameters, lens_parameters);
}

void Locate::locate(const cv::Mat& depth_image, const std::vector<std::vector<BoundingBox>>& bboxes,
                    std::vector<std::vector<float>>& distances, std::vector<std::vector<std::vector<float>>>& points){
#ifdef PROFILE
  start_distance_ = std::chrono::system_clock::now();
#endif
  distances.clear();
  distances = PE_->extractDistanceFromDepth(depth_image, bboxes);
#ifdef PROFILE
  end_distance_ = std::chrono::system_clock::now();
  start_position_ = std::chrono::system_clock::now();
#endif
  points.clear();
  points = PE_->estimatePosition(distances, bboxes);
#ifdef PROFILE
  end_position_ = std::chrono::system_clock::now();
#endif
}

void Locate::locate(const cv::Mat& depth_image, const std::vector<std::map<unsigned int, std::vector<float>>>& states,
                    std::vector<std::map<unsigned int, float>>& distances, std::vector<std::map<unsigned int, std::vector<float>>>& points){
#ifdef PROFILE
  start_distance_ = std::chrono::system_clock::now();
#endif
  distances.clear();
  distances = PE_->extractDistanceFromDepth(depth_image, states);
#ifdef PROFILE
  end_distance_ = std::chrono::system_clock::now();
  start_position_ = std::chrono::system_clock::now();
#endif
  points.clear();
  points = PE_->estimatePosition(distances, states);
#ifdef PROFILE
  end_position_ = std::chrono::system_clock::now();
#endif
}

void Locate::printProfilingLocalization(){
#ifdef PROFILE
  printf(" - Distance estimation done in %ld us\n", std::chrono::duration_cast<std::chrono::microseconds>(end_distance_ - start_distance_).count());
  printf(" - Position estimation done in %ld us\n", std::chrono::duration_cast<std::chrono::microseconds>(end_position_ - start_position_).count());
#endif
}

void Locate::make3DBoundingBoxes(const std::vector<std::vector<std::vector<float>>>& points, const std::vector<std::vector<BoundingBox>>& bboxes,
                                  std::vector<std::vector<BoundingBox3D>>& bboxes3D) {
  float new_w, new_h;
  bboxes3D.clear();
  for (unsigned int i=0; i<points.size(); i++){
    std::vector<BoundingBox3D> tmp_bboxes3D;
    for (unsigned int j=0; j<points[i].size(); j++) {
      new_w = points[i][j][2] * bboxes[i][j].w_ / PE_->getFx();
      new_h = points[i][j][2] * bboxes[i][j].h_ / PE_->getFy();
      BoundingBox3D tmp(points[i][j][0],points[i][j][1],points[i][j][2], new_w, new_w, new_h, bboxes[i][j].confidence_, bboxes[i][j].class_id_);
      tmp_bboxes3D.push_back(tmp);
    }
    bboxes3D.push_back(tmp_bboxes3D);
  }
}

Track2D::Track2D(){}

Track2D::Track2D(DetectionParameters& det_p, KalmanParameters& kal_p, TrackingParameters& tra_p, BBoxRejectionParameters& bbo_p){
  Q_ = kal_p.Q;
  R_ = kal_p.R;
  dist_threshold_ = tra_p.distance_thresh;
  center_threshold_ = tra_p.center_thresh;
  area_threshold_ = tra_p.area_thresh;
  body_ratio_ = tra_p.body_ratio;
  use_dim_ = kal_p.use_dim;
  use_vel_ = kal_p.use_vel;
  dt_ = tra_p.dt;
  max_frames_to_skip_ = tra_p.max_frames_to_skip;
  min_bbox_width_ = bbo_p.min_bbox_width;
  max_bbox_width_ = bbo_p.max_bbox_width;
  min_bbox_height_ = bbo_p.min_bbox_height;
  max_bbox_height_ = bbo_p.max_bbox_width;
  class_map_ = det_p.class_map;

  for (unsigned int i=0; i<det_p.num_classes; i++){ // Create as many trackers as their are classes
    Trackers_.push_back(new Tracker2D(max_frames_to_skip_, dist_threshold_, center_threshold_,
                      area_threshold_, body_ratio_, dt_, use_dim_,
                      use_vel_, Q_, R_)); 
  }
}

void Track2D::buildTrack2D(DetectionParameters& det_p, KalmanParameters& kal_p, TrackingParameters& tra_p, BBoxRejectionParameters& bbo_p){
  Q_ = kal_p.Q;
  R_ = kal_p.R;
  dist_threshold_ = tra_p.distance_thresh;
  center_threshold_ = tra_p.center_thresh;
  area_threshold_ = tra_p.area_thresh;
  body_ratio_ = tra_p.body_ratio;
  use_dim_ = kal_p.use_dim;
  use_vel_ = kal_p.use_vel;
  dt_ = tra_p.dt;
  max_frames_to_skip_ = tra_p.max_frames_to_skip;
  min_bbox_width_ = bbo_p.min_bbox_width;
  max_bbox_width_ = bbo_p.max_bbox_width;
  min_bbox_height_ = bbo_p.min_bbox_height;
  max_bbox_height_ = bbo_p.max_bbox_width;
  class_map_ = det_p.class_map;

  for (unsigned int i=0; i<det_p.num_classes; i++){ // Create as many trackers as their are classes
    Trackers_.push_back(new Tracker2D(max_frames_to_skip_, dist_threshold_, center_threshold_,
                      area_threshold_, body_ratio_, dt_, use_dim_,
                      use_vel_, Q_, R_)); 
  }
}

Track2D::~Track2D() {
  Trackers_.clear();
}
  

void Track2D::cast2states(std::vector<std::vector<std::vector<float>>>& states, const std::vector<std::vector<BoundingBox>>& bboxes) {
  states.clear();
  std::vector<std::vector<float>> state_vec;
  std::vector<float> state(6);

  for (unsigned int i; i < bboxes.size(); i++) {
    state_vec.clear();
    for (unsigned int j; j < bboxes[i].size(); j++) {
      if (!bboxes[i][j].valid_) {
        continue;
      }
      if (bboxes[i][j].h_ > max_bbox_height_) {
        continue;
      }
      if (bboxes[i][j].w_ > max_bbox_width_) {
        continue;
      }
      if (bboxes[i][j].h_ < min_bbox_height_) {
        continue;
      }
      if (bboxes[i][j].w_ < min_bbox_width_) {
        continue;
      }
      state[0] = bboxes[i][j].x_;
      state[1] = bboxes[i][j].y_;
      state[2] = 0;
      state[3] = 0;
      state[4] = bboxes[i][j].w_;
      state[5] = bboxes[i][j].h_;
      state_vec.push_back(state);
      //printf("state %d, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f\", j, state[0], state[1], state[2], state[3], state[4], state[5]);
    }
    states.push_back(state_vec);
  }
}

void Track2D::track(const std::vector<std::vector<BoundingBox>>& bboxes,
               std::vector<std::map<unsigned int, std::vector<float>>>& tracker_states) {
#ifdef PROFILE
  start_tracking_ = std::chrono::system_clock::now();
#endif 
  std::vector<std::vector<std::vector<float>>> states;
  cast2states(states, bboxes);
  for (unsigned int i=0; i < tracker_states.size(); i++){
    std::vector<std::vector<float>> states_to_track;
    states_to_track = states[i];
    Trackers_[i]->update(dt_, states_to_track);
    Trackers_[i]->getStates(tracker_states[i]);
  }
#ifdef PROFILE
  end_tracking_ = std::chrono::system_clock::now();
#endif
}

void Track2D::track(const std::vector<std::vector<BoundingBox>>& bboxes,
               std::vector<std::map<unsigned int, std::vector<float>>>& tracker_states, const float& dt) {
#ifdef PROFILE
  start_tracking_ = std::chrono::system_clock::now();
#endif 
  std::vector<std::vector<std::vector<float>>> states;
  cast2states(states, bboxes);
  for (unsigned int i=0; i < tracker_states.size(); i++){
    std::vector<std::vector<float>> states_to_track;
    states_to_track = states[i];
    Trackers_[i]->update(dt, states_to_track);
    Trackers_[i]->getStates(tracker_states[i]);
  }
#ifdef PROFILE
  end_tracking_ = std::chrono::system_clock::now();
#endif
}

void Track2D::generateTrackingImage(cv::Mat& image, const std::vector<std::map<unsigned int, std::vector<float>>> tracker_states) {
  for (unsigned int i=0; i < tracker_states.size(); i++) {
    for (auto & element : tracker_states[i]) {
      cv::Rect rect(element.second[0] - element.second[4]/2, element.second[1]-element.second[5]/2, element.second[4], element.second[5]);
      cv::rectangle(image, rect, ColorPalette[element.first % 24], 3);
      cv::putText(image, class_map_[i]+" "+std::to_string(element.first), cv::Point(element.second[0]-element.second[4]/2,element.second[1]-element.second[5]/2-10), cv::FONT_HERSHEY_SIMPLEX, 0.9, ColorPalette[element.first % 24], 2);
    }
  }
}

void Track2D::printProfilingTracking(){
#ifdef PROFILE
  printf(" - Tracking done in %ld us\n", std::chrono::duration_cast<std::chrono::microseconds>(end_tracking_ - start_tracking_).count());
#endif
}

Track3D::Track3D(){}

Track3D::Track3D(DetectionParameters& det_p, KalmanParameters& kal_p, TrackingParameters& tra_p, BBoxRejectionParameters& bbo_p){
  Q_ = kal_p.Q;
  R_ = kal_p.R;
  dist_threshold_ = tra_p.distance_thresh;
  center_threshold_ = tra_p.center_thresh;
  area_threshold_ = tra_p.area_thresh;
  body_ratio_ = tra_p.body_ratio;
  use_dim_ = kal_p.use_dim;
  use_vel_ = kal_p.use_vel;
  dt_ = tra_p.dt;
  max_frames_to_skip_ = tra_p.max_frames_to_skip;
  min_bbox_width_ = bbo_p.min_bbox_width;
  max_bbox_width_ = bbo_p.max_bbox_width;
  min_bbox_height_ = bbo_p.min_bbox_height;
  max_bbox_height_ = bbo_p.max_bbox_width;
  class_map_ = det_p.class_map;

  for (unsigned int i=0; i<det_p.num_classes; i++){ // Create as many trackers as their are classes
    Trackers_.push_back(new Tracker3D(max_frames_to_skip_, dist_threshold_, center_threshold_,
                      area_threshold_, body_ratio_, dt_, use_dim_,
                      use_vel_, Q_, R_)); 
  }
}

void Track3D::buildTrack3D(DetectionParameters& det_p, KalmanParameters& kal_p, TrackingParameters& tra_p, BBoxRejectionParameters& bbo_p){
  Q_ = kal_p.Q;
  R_ = kal_p.R;
  dist_threshold_ = tra_p.distance_thresh;
  center_threshold_ = tra_p.center_thresh;
  area_threshold_ = tra_p.area_thresh;
  body_ratio_ = tra_p.body_ratio;
  use_dim_ = kal_p.use_dim;
  use_vel_ = kal_p.use_vel;
  dt_ = tra_p.dt;
  max_frames_to_skip_ = tra_p.max_frames_to_skip;
  min_bbox_width_ = bbo_p.min_bbox_width;
  max_bbox_width_ = bbo_p.max_bbox_width;
  min_bbox_height_ = bbo_p.min_bbox_height;
  max_bbox_height_ = bbo_p.max_bbox_width;
  class_map_ = det_p.class_map;

  for (unsigned int i=0; i<det_p.num_classes; i++){ // Create as many trackers as their are classes
    Trackers_.push_back(new Tracker3D(max_frames_to_skip_, dist_threshold_, center_threshold_,
                      area_threshold_, body_ratio_, dt_, use_dim_,
                      use_vel_, Q_, R_)); 
  }
}

Track3D::~Track3D() {
  Trackers_.clear();
} 

void Track3D::cast2states(std::vector<std::vector<std::vector<float>>>& states, const std::vector<std::vector<BoundingBox3D>>& bboxes) {
  states.clear();
  std::vector<std::vector<float>> state_vec;
  std::vector<float> state(6);

  for (unsigned int i; i < bboxes.size(); i++) {
    state_vec.clear();
    for (unsigned int j; j < bboxes[i].size(); j++) {
      if (!bboxes[i][j].valid_) {
        continue;
      }
      if (bboxes[i][j].h_ > max_bbox_height_) {
        continue;
      }
      if (bboxes[i][j].w_ > max_bbox_width_) {
        continue;
      }
      if (bboxes[i][j].h_ < min_bbox_height_) {
        continue;
      }
      if (bboxes[i][j].w_ < min_bbox_width_) {
        continue;
      }
      state[0] = bboxes[i][j].x_;
      state[1] = bboxes[i][j].y_;
      state[2] = bboxes[i][j].z_;
      state[3] = 0;
      state[4] = 0;
      state[5] = 0;
      state[6] = bboxes[i][j].w_;
      state[7] = bboxes[i][j].d_;
      state[8] = bboxes[i][j].h_;
      state_vec.push_back(state);
      //printf("state %d, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f\", j, state[0], state[1], state[2], state[3], state[4], state[5]);
    }
    states.push_back(state_vec);
  }
}


void Track3D::track(const std::vector<std::vector<BoundingBox3D>>& bboxes,
               std::vector<std::map<unsigned int, std::vector<float>>>& tracker_states) {
#ifdef PROFILE
  start_tracking_ = std::chrono::system_clock::now();
#endif 
  std::vector<std::vector<std::vector<float>>> states;
  cast2states(states, bboxes);
  for (unsigned int i=0; i < tracker_states.size(); i++){
    std::vector<std::vector<float>> states_to_track;
    states_to_track = states[i];
    Trackers_[i]->update(dt_, states_to_track);
    Trackers_[i]->getStates(tracker_states[i]);
  }
#ifdef PROFILE
  end_tracking_ = std::chrono::system_clock::now();
#endif
}

void Track3D::track(const std::vector<std::vector<BoundingBox3D>>& bboxes,
               std::vector<std::map<unsigned int, std::vector<float>>>& tracker_states, const float& dt) {
#ifdef PROFILE
  start_tracking_ = std::chrono::system_clock::now();
#endif 
  std::vector<std::vector<std::vector<float>>> states;
  cast2states(states, bboxes);
  for (unsigned int i=0; i < tracker_states.size(); i++){
    std::vector<std::vector<float>> states_to_track;
    states_to_track = states[i];
    Trackers_[i]->update(dt, states_to_track);
    Trackers_[i]->getStates(tracker_states[i]);
  }
#ifdef PROFILE
  end_tracking_ = std::chrono::system_clock::now();
#endif
}

void Track3D::generateTrackingImage(cv::Mat& image, const std::vector<std::map<unsigned int, std::vector<float>>> tracker_states) {
  for (unsigned int i=0; i < tracker_states.size(); i++) {
    for (auto & element : tracker_states[i]) {
      cv::Rect rect(element.second[0] - element.second[4]/2, element.second[1]-element.second[5]/2, element.second[4], element.second[5]);
      cv::rectangle(image, rect, ColorPalette[element.first % 24], 3);
      cv::putText(image, class_map_[i]+" "+std::to_string(element.first), cv::Point(element.second[0]-element.second[4]/2,element.second[1]-element.second[5]/2-10), cv::FONT_HERSHEY_SIMPLEX, 0.9, ColorPalette[element.first % 24], 2);
    }
  }
}

void Track3D::printProfilingTracking(){
#ifdef PROFILE
  printf(" - Tracking done in %ld us\n", std::chrono::duration_cast<std::chrono::microseconds>(end_tracking_ - start_tracking_).count());
#endif
}

DetectAndLocate::DetectAndLocate() : Detect(), Locate(){}
DetectAndLocate::DetectAndLocate(GlobalParameters& glo_p, DetectionParameters& det_p, NMSParameters& nms_p,
      LocalizationParameters& loc_p, CameraParameters& cam_p) : Detect(glo_p, det_p, nms_p), Locate(glo_p, loc_p, cam_p){}
void DetectAndLocate::applyOnFolder(std::string, std::string, bool, bool, bool) {}
void DetectAndLocate::applyOnVideo(std::string, std::string, bool, bool, bool) {}

DetectAndTrack2D::DetectAndTrack2D() : Detect(), Track2D(){}
DetectAndTrack2D::DetectAndTrack2D(GlobalParameters& glo_p, DetectionParameters& det_p, NMSParameters& nms_p,
      KalmanParameters& kal_p, TrackingParameters& tra_p, BBoxRejectionParameters& bbo_p) : Detect(glo_p,
      det_p, nms_p), Track2D(det_p, kal_p, tra_p, bbo_p){}
void DetectAndTrack2D::applyOnFolder(std::string, std::string, bool, bool, bool) {}
void DetectAndTrack2D::applyOnVideo(std::string, std::string, bool, bool, bool) {}

DetectTrack2DAndLocate::DetectTrack2DAndLocate() : Detect(), Track2D(), Locate(){}
DetectTrack2DAndLocate::DetectTrack2DAndLocate(GlobalParameters& glo_p, DetectionParameters& det_p,
      NMSParameters& nms_p, KalmanParameters& kal_p, TrackingParameters& tra_p, BBoxRejectionParameters& bbo_p, 
      LocalizationParameters& loc_p, CameraParameters& cam_p) : Detect(glo_p, det_p, nms_p), Track2D(det_p, kal_p, 
      tra_p, bbo_p), Locate(glo_p, loc_p, cam_p){}
void DetectTrack2DAndLocate::applyOnFolder(std::string, std::string, bool, bool, bool) {}
void DetectTrack2DAndLocate::applyOnVideo(std::string, std::string, bool, bool, bool) {}