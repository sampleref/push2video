//
// Created by chakra on 29-03-2020.
//

#ifndef PUSHTOTALKSERVICE_VIDEOPIPELINEHANDLER_HPP
#define PUSHTOTALKSERVICE_VIDEOPIPELINEHANDLER_HPP

#include <gst/gst.h>
#include <string>
#include <map>
#include <list>

using namespace std;

class WebRtcPeer;

typedef std::shared_ptr<WebRtcPeer> WebRtcPeerPtr;

class VideoPipelineHandler {
public:
    //Attributes
    std::string channelId;
    GstElement *pipeline;
    GMainLoop *loop;
    std::string senderPeerId;
    WebRtcPeerPtr senderPeer;
    bool audio_valid = false;
    bool video_valid = false;
    bool apply_watchdog = false;
    std::map<std::string, WebRtcPeerPtr> peerVideoReceivers; //Peers receiving common video from pipeline

    //Methods
    void
    create_video_pipeline_sender_peer(std::string channelId, std::string senderPeerId, std::list<std::string> receivers,
                                      std::string sdp, std::string type);

    gboolean start_pipeline();

    void apply_webrtc_video_sender_sdp(std::string peerId, std::string sdp, std::string type);

    void apply_webrtc_video_receiver_sdp(std::string peerId, std::string sdp, std::string type);

    void apply_webrtc_video_sender_ice(std::string peerId, std::string ice, std::string mLineIndex);

    void apply_webrtc_video_receiver_ice(std::string peerId, std::string ice, std::string mLineIndex);

    void send_webrtc_video_sender_sdp(std::string peerId, std::string sdp, std::string type);

    void send_webrtc_video_receiver_sdp(std::string peerId, std::string sdp, std::string type);

    void send_webrtc_video_sender_ice(std::string peerId, std::string ice, std::string mLineIndex);

    void send_webrtc_video_receiver_ice(std::string peerId, std::string ice, std::string mLineIndex);

    void remove_webrtc_video_peer(std::string peerId);

    void remove_sender_peer_close_pipeline();

    void create_receivers_for_video();


private:
    WebRtcPeerPtr fetch_video_reciever_by_peerid(std::string peerId);

    void remove_video_reciever_by_peerid(std::string peerId);

    void remove_receiver_peer_from_pipeline(std::string peerId, bool remove_from_list);
};

typedef std::shared_ptr<VideoPipelineHandler> VideoPipelineHandlerPtr;

#endif //PUSHTOTALKSERVICE_VIDEOPIPELINEHANDLER_HPP
