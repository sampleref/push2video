//
// Created by chakra on 03-03-2020.
//

#ifndef PUSHTOTALKSERVICE_WEBRTCAUDIOPEER_HPP
#define PUSHTOTALKSERVICE_WEBRTCAUDIOPEER_HPP

#include <gst/gst.h>
#include <memory>
#include <string>

using namespace std;

enum WebRtcAudioPeerDirection {
    RECEIVER = 0,
    SENDER = 1
};

class WebRtcAudioPeer {

public:
    //Attributes
    std::string peerId;
    std::string meetingId;
    WebRtcAudioPeerDirection audioPeerDirection;
    GstElement *webrtcAudio;
    std::string sdp;

    //Methods
    gboolean isValidSdp(void);
};

typedef std::shared_ptr<WebRtcAudioPeer> WebRtcAudioPeerPtr;

#endif //PUSHTOTALKSERVICE_WEBRTCAUDIOPEER_HPP
