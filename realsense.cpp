// based on https://github.com/IntelRealSense/librealsense/tree/master/examples/align
// tested with librealsense2-2.10.4

#include <librealsense2/rs.hpp>
#include <iostream>
#include "common.h"

float get_depth_scale(rs2::device dev) {
  // Go over the device's sensors
  for (rs2::sensor& sensor : dev.query_sensors()) {
    // Check if the sensor if a depth sensor
    if (rs2::depth_sensor dpt = sensor.as<rs2::depth_sensor>()) {
      return dpt.get_depth_scale();
    }
  }
  throw std::runtime_error("Device does not have a depth sensor");
}

int main(int argc, char* argv[]) {

  bool _quit = false; quit = &_quit;
  if (argc > 2) gstpipe = argv[2];

  opencv_init();
  gstreamer_init(argc,argv,"RGB");

  // Create a Pipeline - this serves as a top-level API for streaming and processing frames
  rs2::pipeline pipe;
  rs2::config cfg;
  rs2::colorizer color_map;

  cfg.enable_stream(RS2_STREAM_DEPTH, 1280, 720, RS2_FORMAT_Z16, 30);
  cfg.enable_stream(RS2_STREAM_COLOR, 1280, 720, RS2_FORMAT_RGB8, 30);

  // Configure and start the pipeline
  rs2::pipeline_profile profile = pipe.start( cfg );
  float depth_scale = get_depth_scale(profile.get_device());

  // Create a rs2::align object.
  // rs2::align allows us to perform alignment of depth frames to others frames
  //The "align_to" is the stream type to which we plan to align depth frames.
  rs2::align align(RS2_STREAM_COLOR);

  while (!_quit) {

    // Block program until frames arrive
    rs2::frameset frames = pipe.wait_for_frames(); 
    
    //Get processed aligned frame
    auto processed = align.process(frames);

    rs2::video_frame color_frame = processed.first(RS2_STREAM_COLOR);

    // Try to get a frame of a depth image
    rs2::depth_frame depth = processed.get_depth_frame(); 

    //rs2::video_frame color_frame = color_map(depth);

    // Get the depth frame's dimensions
    int width = depth.get_width();
    int height = depth.get_height();
    int other_bpp = color_frame.get_bytes_per_pixel();

		float clipping_dist = 1.0;

    const uint16_t* p_depth_frame = reinterpret_cast<const uint16_t*>(depth.get_data());
    uint8_t* p_other_frame = reinterpret_cast<uint8_t*>(const_cast<void*>(color_frame.get_data()));

    for (int y = 0; y < height; y++) {
        auto depth_pixel_index = y * width;
        for (int x = 0; x < width; x++, ++depth_pixel_index) {
            // Get the depth value of the current pixel
            auto pixels_distance = depth_scale * p_depth_frame[depth_pixel_index];

            // Check if the depth value is invalid (<=0) or greater than the threashold
            if (pixels_distance <= 0.f || pixels_distance > clipping_dist) {
                // Calculate the offset in other frame's buffer to current pixel
                auto offset = depth_pixel_index * other_bpp;
                // Set pixel to "background" color (0x999999)
                std::memset(&p_other_frame[offset], 0x99, other_bpp);
            }
        }
		}
    
    // Query the distance from the camera to the object in the center of the image
    float dist_to_center = depth.get_distance(width / 2, height / 2);
    
    // Print the distance 
    std::cout << "The camera is facing an object " << dist_to_center << " meters away \r";

    cv::Mat input(color_frame.get_height(),color_frame.get_width(),CV_8UC3,(uint8_t*)color_frame.get_data());
    prepare_buffer(&input,1280,720,CV_8UC3);
  }

	gstreamer_cleanup();
}