//
// Created by kantipud on 10-04-2020.
//

#ifndef PUSHTOTALKSERVICE_GSTMEDIAUTILS_HPP
#define PUSHTOTALKSERVICE_GSTMEDIAUTILS_HPP
#define GST_USE_UNSTABLE_API

#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>
#include <memory>
#include "string"
#include "string.h"

const std::string DEF_AUDIO_CODEC = "OPUS";
const std::string DEF_VIDEO_CODEC = "VP8";

const int WATCHDOG_MILLI_SECS = 15000;

class SdpMediaState {
public:
    bool valid = false;
    bool audio = false;
    bool video = false;
};

typedef std::shared_ptr<SdpMediaState> SdpMediaStatePtr;

namespace gstMediaUtils {
    SdpMediaStatePtr load_media_state_in_sdp(std::string sdpStr);
}

#endif //PUSHTOTALKSERVICE_GSTMEDIAUTILS_HPP
