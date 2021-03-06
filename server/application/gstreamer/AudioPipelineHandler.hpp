//
// Created by chakra on 03-03-2020.
//

#ifndef PUSHTOTALKSERVICE_AUDIOPIPELINEHANDLER_HPP
#define PUSHTOTALKSERVICE_AUDIOPIPELINEHANDLER_HPP

#include <gst/gst.h>
#include <string>
#include <map>


using namespace std;

class WebRtcPeer;

typedef std::shared_ptr<WebRtcPeer> WebRtcPeerPtr;

class AudioPipelineHandler {

public:
    //Attributes
    std::string channelId;
    GstElement *pipeline;
    GMainLoop *loop;
    std::map<std::string, WebRtcPeerPtr> peerAudioReceivers; //Peers receiving common audio from pipeline
    std::map<std::string, WebRtcPeerPtr> peerAudioSenders; //Peers sending audio to pipeline

    //Methods
    WebRtcPeerPtr fetch_audio_sender_by_peerid(std::string peerId);

    WebRtcPeerPtr fetch_audio_reciever_by_peerid(std::string peerId);

    void remove_audio_sender_by_peerid(std::string peerId);

    void remove_audio_reciever_by_peerid(std::string peerId);

    gboolean start_pipeline();

    void add_webrtc_audio_sender_receiver(std::string peerId);

    void remove_webrtc_audio_sender_receiver(std::string peerId);

    void apply_webrtc_audio_sender_sdp(std::string peerId, std::string sdp, std::string type);

    void apply_webrtc_audio_receiver_sdp(std::string peerId, std::string sdp, std::string type);

    void apply_webrtc_audio_sender_ice(std::string peerId, std::string ice, std::string mLineIndex);

    void apply_webrtc_audio_receiver_ice(std::string peerId, std::string ice, std::string mLineIndex);

    void send_webrtc_audio_sender_sdp(std::string peerId, std::string sdp, std::string type);

    void send_webrtc_audio_receiver_sdp(std::string peerId, std::string sdp, std::string type);

    void send_webrtc_audio_sender_ice(std::string peerId, std::string ice, std::string mLineIndex);

    void send_webrtc_audio_receiver_ice(std::string peerId, std::string ice, std::string mLineIndex);
};

typedef std::shared_ptr<AudioPipelineHandler> AudioPipelineHandlerPtr;

#endif //PUSHTOTALKSERVICE_AUDIOPIPELINEHANDLER_HPP
