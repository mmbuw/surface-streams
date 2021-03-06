#include "Camera.h"

#include <vector>
#include <iostream>
#include <unistd.h>

////////////////////////////////////////////////////////////////////////////////
//
// OpenCV stuff
//

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace cv;

// final transmitted image dimensions
const int tw = 1280, th = 720;

Camera::Camera(const char* _pipe, const char* _type, int _cw, int _ch, int _dw, int _dh) {
  dw = _dw; dh = _dh; cw = _cw; ch = _ch;
  im = (Mat_<float>(3,3) << (float)tw/(float)cw, 0, 0, 0, (float)th/(float)ch, 0, 0, 0, 1 );
  cv::FileStorage file("perspective.xml", cv::FileStorage::READ);
  file["perspective"] >> pm;
  if (!file.isOpened()) pm = im;

  gstreamer_init(_type,_pipe);
  find_plane = false;
  do_filter = true;
  do_quit = false;
  distance = 1.0f; // in cm
}

Mat Camera::calcPerspective() {

  Mat result;

  dst.push_back(Point2f( 0, 0));
  dst.push_back(Point2f(tw, 0));
  dst.push_back(Point2f(tw,th));
  dst.push_back(Point2f( 0,th));

  result = getPerspectiveTransform(src,dst);

  cv::FileStorage file("perspective.xml", cv::FileStorage::WRITE);
  file << "perspective" << result;

  src.clear();
  dst.clear();

  return result;
}

////////////////////////////////////////////////////////////////////////////////
//
// plane model stuff
//

#include <Eigen/Core>
#include <SimpleRansac.h>
#include <PlaneModel.h>

// use RANSAC to compute a plane out of sparse point cloud
void Camera::ransac_plane() {

  std::vector<Eigen::Vector3f> points;

  for (int y = 0; y < dh; y++) {
    for (int x = 0; x < dw; x++) {
      float pt[3];
      get_3d_pt(x,y,pt);
      if (std::isnan(pt[2]) || std::isinf(pt[2]) || pt[2] <= 0) continue;
      Eigen::Vector3f point = { pt[0], pt[1], pt[2] };
      points.push_back( point );
    }
  }

  std::cout << "3D point count: " << points.size() << std::endl;
  plane = ransac<PlaneModel<float>>( points, distance*10, 200 ); // FIXME distance multiplier is device-specific
  if (plane.d < 0.0) { plane.d = -plane.d; plane.n = -plane.n; }
  std::cout << "Ransac computed plane: n=" << plane.n.transpose() << " d=" << plane.d << std::endl;
}

void Camera::push_point(float x, float y) {
  src.push_back(Point2f((float)cw*x/(float)tw,(float)ch*y/(float)th));
  if (src.size() == 4) {
    Mat r = calcPerspective();
    pm = r;
  }
}

void Camera::get_3d_pt(int x, int y, float* out) {
  out[0] = out[1] = out[2] = 0.0f;
}

////////////////////////////////////////////////////////////////////////////////
//
// GStreamer stuff
//

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/navigation.h>

#include <stdint.h>
#include <string.h>

gboolean pad_event(GstPad *pad, GstObject *parent, GstEvent *event) {

  if (GST_EVENT_TYPE (event) != GST_EVENT_NAVIGATION)
    return gst_pad_event_default(pad,parent,event);

  double x,y; int b;
  const gchar* key;
  Camera* cam = (Camera*)gst_pad_get_element_private(pad);

  switch (gst_navigation_event_get_type(event)) {

    // calibration: top (left, right), bottom (left, right)
    case GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE:
      gst_navigation_event_parse_mouse_button_event(event,&b,&x,&y);
      cam->push_point(x,y);
      break;

    case GST_NAVIGATION_EVENT_KEY_PRESS:
      gst_navigation_event_parse_key_event(event,&key);
      cam->handle_key(key);
      break;

    default:
      return false;
  }

  return true;
}

void Camera::handle_key(const char* key) {

      // reset perspective matrix
      if (key == std::string("space"))
        pm = im;

      // find largest plane
      if (key == std::string("p"))
        find_plane = true;

      // subtract plane
      if (key == std::string("f"))
        do_filter = !do_filter;

      // quit
      if (key == std::string("q"))
        do_quit = true;

      // change plane distance threshold
      if (key == std::string( "plus")) distance += 0.2;
      if (key == std::string("minus")) distance -= 0.2;

      std::cout << "current distance: " << distance << std::endl;
}

void Camera::gstreamer_init(const char* type, const char* gstpipe) {

  /* init GStreamer */
  gst_init (nullptr, nullptr);

  /* setup pipeline */
  gpipeline = gst_pipeline_new ("pipeline");
  appsrc = gst_element_factory_make ("appsrc", "source");

  // create pipeline from string
  const char* pipe_desc = gstpipe ? gstpipe : "videoconvert ! fpsdisplaysink sync=false";
  std::cout << "creating pipeline: " << pipe_desc << std::endl;
  videosink = gst_parse_bin_from_description(pipe_desc,TRUE,NULL);

  /* setup */
  g_object_set (G_OBJECT (appsrc), "caps",
    gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, type,
      "width",  G_TYPE_INT, tw,
      "height", G_TYPE_INT, th,
      "framerate", GST_TYPE_FRACTION, 0, 1,
    NULL), NULL);
  gst_bin_add_many (GST_BIN (gpipeline), appsrc, videosink, NULL);
  gst_element_link_many (appsrc, videosink, NULL);

  // attach event listener to suitable src pad (either appsrc or videoconvert)
  // FIXME: ugly hack, hardcoded element name
  GstElement* display = gst_bin_get_by_name( (GstBin*)gpipeline, "display" );
  GstPad* srcpad = gst_element_get_static_pad (display?display:appsrc, "src");
  gst_pad_set_event_function( srcpad, (GstPadEventFunction)pad_event );
  gst_pad_set_element_private( srcpad, (gpointer)this);

  /* setup appsrc */
  g_object_set (G_OBJECT (appsrc),
    "stream-type", 0, // GST_APP_STREAM_TYPE_STREAM
    "format", GST_FORMAT_TIME,
    "is-live", TRUE,
    "block", TRUE,
    "do-timestamp", TRUE,
    NULL);

  /* play */
  gst_element_set_state (gpipeline, GST_STATE_PLAYING);
}

void buffer_destroy(gpointer data) {
  cv::Mat* done = (cv::Mat*)data;
  delete done;
}

void Camera::send_buffer() {

  if (input.total() == 0) return;

  Mat* output = new Mat(th,tw,input.type());
  warpPerspective(input,*output,pm,output->size(),INTER_NEAREST);

  guint size = output->total()*output->elemSize();
  gpointer data = output->data;
  void* frame = output;

  //prepare_buffer(IN_W*IN_H*4,output->data,output);

  GstBuffer *buffer = gst_buffer_new_wrapped_full( (GstMemoryFlags)0, data, size, 0, size, frame, buffer_destroy );

  gst_app_src_push_buffer((GstAppSrc*)appsrc, buffer); // ignoring GstFlowReturn result for now

  g_main_context_iteration(g_main_context_default(),FALSE);
}

void Camera::gstreamer_cleanup() {
  /* clean up */
  std::cout << "cleaning up" << std::endl;
  gst_element_set_state (gpipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (gpipeline));
}

void Camera::retrieve_frames() {
}

void Camera::remove_background() {
}
