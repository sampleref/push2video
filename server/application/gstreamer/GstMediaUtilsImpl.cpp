//
// Created by kantipud on 10-04-2020.
//

#include "GstMediaUtils.hpp"

GST_DEBUG_CATEGORY_EXTERN (push2talk_gst);
#define GST_CAT_DEFAULT push2talk_gst

namespace gstMediaUtils {

    SdpMediaStatePtr load_media_state_in_sdp(std::string sdpStr) {
        SdpMediaStatePtr mediaStatePtr = std::make_shared<SdpMediaState>();
        GstSDPMessage *sdpMsg;
        int ret;
        ret = gst_sdp_message_new(&sdpMsg);
        if (GST_SDP_OK != ret) {
            GST_ERROR("Error in creating new sdp instance");
            mediaStatePtr->valid = false;
            return mediaStatePtr;
        }
        ret = gst_sdp_message_parse_buffer((guint8 *) sdpStr.c_str(), strlen(sdpStr.c_str()), sdpMsg);
        if (GST_SDP_OK != ret) {
            GST_ERROR("Error in parsing sdp string to instance %d ", ret);
            mediaStatePtr->valid = false;
            return mediaStatePtr;
        }
        gst_sdp_message_free(sdpMsg);
        if (strstr(sdpStr.c_str(), "m=audio") && strcasestr(sdpStr.c_str(), DEF_AUDIO_CODEC.c_str())) {
            mediaStatePtr->valid = true;
            mediaStatePtr->audio = true;
        }
        if (strstr(sdpStr.c_str(), "m=video") && strcasestr(sdpStr.c_str(), DEF_VIDEO_CODEC.c_str())) {
            mediaStatePtr->valid = true;
            mediaStatePtr->video = true;
        }
        return mediaStatePtr;
    }

}