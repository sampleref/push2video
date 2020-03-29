//
// Created by chakra on 03-03-2020.
//

#ifndef PUSHTOTALKSERVICE_WEBRTCPEER_HPP
#define PUSHTOTALKSERVICE_WEBRTCPEER_HPP

#include <gst/gst.h>
#include <memory>
#include <string>

using namespace std;

enum WebRtcPeerDirection {
    RECEIVER = 0,
    SENDER = 1
};

enum WebRtcMediaType {
    AUDIO = 0,
    VIDEO = 1
};

class WebRtcPeer {

public:
    //Attributes
    std::string peerId;
    std::string channelId;
    WebRtcPeerDirection peerDirection;
    WebRtcMediaType webRtcMediaType;
    GstElement *webrtcElement;
    std::string sdp;
    std::string sdp_type;

    //Methods
    gboolean isValidSdp(void);
};

typedef std::shared_ptr<WebRtcPeer> WebRtcPeerPtr;

#endif //PUSHTOTALKSERVICE_WEBRTCPEER_HPP
