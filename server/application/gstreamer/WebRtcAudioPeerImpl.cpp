//
// Created by chakra on 03-03-2020.
//

#include "WebRtcAudioPeer.hpp"
#include "../utils/Push2TalkUtils.hpp"

GST_DEBUG_CATEGORY_EXTERN (push2talk_gst);
#define GST_CAT_DEFAULT push2talk_gst

gboolean WebRtcAudioPeer::isValidSdp(void) {
    return TRUE;
}
