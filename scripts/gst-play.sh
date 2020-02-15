#!/bin/bash
gst-launch-1.0 -ve filesrc location=20200211-152032.mpeg ! tsparse ! tsdemux name=demux \
	demux.video_0_0041 ! queue ! h264parse ! avdec_h264 ! videoconvert ! fpsdisplaysink \
	demux.audio_0_0043 ! queue ! opusparse ! opusdec ! autoaudiosink \
	#demux.video_0_0041 ! queue ! h264parse ! avdec_h264 ! videoconvert ! fpsdisplaysink sync=false \