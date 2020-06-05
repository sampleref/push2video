//
// Created by chakra on 16-05-2020.
//

#ifndef PUSHTOTALKSERVICE_GROUPSENDRECVHANDLER_H
#define PUSHTOTALKSERVICE_GROUPSENDRECVHANDLER_H

#include <gst/gst.h>
#include <string>
#include <map>
#include <list>

using namespace std;

class WebRtcPeer;

typedef std::shared_ptr<WebRtcPeer> WebRtcPeerPtr;

class GroupSendRecvHandler {
public:
    //Attributes
    std::string groupId;
    GstElement *pipeline;
    GMainLoop *loop;
    std::string currentSenderUeId;
    std::map<std::string, WebRtcPeerPtr> senderPeers; //Peers sending av to pipeline
    std::map<std::string, WebRtcPeerPtr> receiverPeers; //Peers receiving av from pipeline

    //Methods
    bool create_group(std::string groupId);

    void av_add_ue(std::string ueId, UeMediaDirection ueMediaDirection);

    void apply_webrtc_peer_sdp(std::string ueId, std::string sdp, std::string type, UeMediaDirection ueMediaDirection);

    void ue_ice_message(std::string icecandidate, std::string mlineindex, std::string ueId,
                        UeMediaDirection ueMediaDirection);

    UeFloorControlResponse_Action
    update_ue_floor_control(std::string ueId, UeFloorControlRequest_Action floorControlRequestAction);

    void reset_ue(std::string ueId, bool removeFromPeers, UeMediaDirection_Direction ueMediaDirection);

    void send_sdp_offer(std::string sdp, std::string peerId);

    void apply_incoming_sdp(std::string peerId, std::string sdp, std::string type);

    void send_webrtc_answer_sdp(std::string ueId, std::string sdp, UeMediaDirection_Direction direction);

    void send_webrtc_ice_message(std::string ueId, std::string ice, std::string mLineIndex,
                                 UeMediaDirection_Direction direction);

    void close_pipeline_group();

    void send_key_frame_request_to_sender();

private:
    //Methods
    void remove_peer_by_ueid(std::string ueId, UeMediaDirection_Direction direction);

    WebRtcPeerPtr fetch_peer_by_ueid(std::string ueId, UeMediaDirection_Direction direction);

    void allocate_floor_control_to_ue(std::string ueId);

    void revoke_floor_control_from_ue(std::string ueId);
};


#endif //PUSHTOTALKSERVICE_GROUPSENDRECVHANDLER_H
