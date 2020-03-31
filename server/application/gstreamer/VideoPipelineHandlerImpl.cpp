//
// Created by chakra on 29-03-2020.
//

#define GST_USE_UNSTABLE_API

#include <thread>
#include <string>
#include <future> // std::async, std::future
#include <chrono>
#include <gst/webrtc/webrtc.h>

#include "VideoPipelineHandler.hpp"
#include "../grpc/GrpcService.hpp"
#include "../utils/Push2TalkUtils.hpp"
#include "WebRtcPeer.hpp"

GST_DEBUG_CATEGORY_EXTERN (push2talk_gst);
#define GST_CAT_DEFAULT push2talk_gst

#define STUN_SERVER " stun-server=stun://stun.l.google.com:19302 "
#define RTP_CAPS_OPUS "application/x-rtp,media=audio,encoding-name=OPUS,payload="
#define RTP_CAPS_VP8 "application/x-rtp,media=video,encoding-name=VP8,payload=96"
#define RTP_CAPS_H264 "application/x-rtp,media=video,encoding-name=H264,payload=96"

void send_video_reset(VideoPipelineHandler *videoPipelineHandler) {
    PeerMessageRequest peerMessageRequest;
    peerMessageRequest.mutable_peerstatusmessage()->set_status(PeerStatusMessage_Status_VIDEO_RESET);
    peerMessageRequest.set_peerid(videoPipelineHandler->senderPeerId);
    peerMessageRequest.set_channelid(videoPipelineHandler->channelId);
    push2talkUtils::pushToTalkServiceClientPtr->sendPeerMessage(peerMessageRequest);
}

void release_video_pipeline(VideoPipelineHandler *videoPipelineHandler) {
    send_video_reset(videoPipelineHandler);
    videoPipelineHandler->remove_sender_peer_close_pipeline();
}

gboolean pipeline_bus_callback_video(GstBus *bus, GstMessage *message, gpointer data) {
    switch (GST_MESSAGE_TYPE (message)) {
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug;
            gst_message_parse_error(message, &err, &debug);
            VideoPipelineHandler *pipelineHandler = static_cast<VideoPipelineHandler *>(data);
            GST_ERROR("pipeline_bus_callback:GST_MESSAGE_ERROR Error/code : %s/%d for meeting %s ", err->message,
                      err->code, pipelineHandler->channelId.c_str());
            release_video_pipeline(pipelineHandler);
            if (err->code > 0) {
                g_error_free(err);
                g_free(debug);
                return FALSE;
            }
            g_error_free(err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_EOS: {
            VideoPipelineHandler *pipelineHandler = static_cast<VideoPipelineHandler *>(data);
            GST_ERROR("pipeline_bus_callback:GST_MESSAGE_EOS for meeting %s ", pipelineHandler->channelId.c_str());
            release_video_pipeline(pipelineHandler);
            return FALSE;
        }
        case GST_MESSAGE_STATE_CHANGED: {
            GstState old_state, new_state;
            gst_message_parse_state_changed(message, &old_state, &new_state, NULL);
            GST_DEBUG("pipeline_bus_callback:GST_MESSAGE_STATE_CHANGED- Element %s changed state from %s to %s. ",
                      GST_OBJECT_NAME(message->src),
                      gst_element_state_get_name(old_state),
                      gst_element_state_get_name(new_state));
            break;
        }
        case GST_MESSAGE_STREAM_STATUS: {
            GstStreamStatusType statusType;
            gst_message_parse_stream_status(message, &statusType, NULL);
            GST_DEBUG("pipeline_bus_callback:GST_MESSAGE_STREAM_STATUS- %s. ", "");
            break;
        }
        case GST_MESSAGE_BUFFERING: {
            gint percent;
            gst_message_parse_buffering(message, &percent);
            GST_DEBUG("pipeline_bus_callback:GST_MESSAGE_BUFFERING- Element %s buffering at %d percent.\n",
                      GST_OBJECT_NAME(message->src), percent);
        }
        default: {
            //GST_INFO("pipeline_bus_callback:default Got %s message ", GST_MESSAGE_TYPE_NAME (message));
            break;
        }
    }
    return TRUE;
}

void VideoPipelineHandler::create_video_pipeline_sender_peer(std::string channelId, std::string senderPeerId,
                                                             std::list<std::string> receivers,
                                                             std::string sdp, std::string type) {
    this->channelId = channelId;
    WebRtcPeerPtr webRtcPeerPtr = std::make_shared<WebRtcPeer>();
    webRtcPeerPtr->webRtcMediaType = VIDEO;
    webRtcPeerPtr->peerDirection = SENDER;
    webRtcPeerPtr->peerId = senderPeerId;
    webRtcPeerPtr->channelId = channelId;
    webRtcPeerPtr->sdp = sdp;
    webRtcPeerPtr->sdp_type = type;
    senderPeer = webRtcPeerPtr;
    this->senderPeerId = senderPeerId;
    for (std::string receiver : receivers) {
        GST_INFO("Creating sender (%s) ===>>> receiver (%s) ", senderPeerId.c_str(), receiver.c_str());
        WebRtcPeerPtr webRtcPeerPtrVideoRecv = std::make_shared<WebRtcPeer>();
        webRtcPeerPtrVideoRecv->webRtcMediaType = VIDEO;
        webRtcPeerPtrVideoRecv->peerDirection = RECEIVER;
        webRtcPeerPtrVideoRecv->channelId = channelId;
        webRtcPeerPtrVideoRecv->peerId = receiver;
        peerVideoReceivers[webRtcPeerPtrVideoRecv->peerId] = webRtcPeerPtrVideoRecv;
    }
    GST_INFO("Number of receivers %lu for sender peer %s ", peerVideoReceivers.size(), senderPeerId.c_str());
    start_pipeline();
}

void send_ice_candidate_message_video(GstElement *webrtc G_GNUC_UNUSED, guint mlineindex,
                                      gchar *candidate, WebRtcPeer *user_data G_GNUC_UNUSED) {
    GST_DEBUG("send_ice_candidate_message of peer/direction/index %s / %d / %d ", candidate,
              user_data->peerDirection, mlineindex);
    VideoPipelineHandlerPtr videoPipelineHandlerPtr = push2talkUtils::fetch_video_pipelinehandler_by_key(
            user_data->channelId);
    if (SENDER == user_data->peerDirection) {
        videoPipelineHandlerPtr->send_webrtc_video_sender_ice(user_data->peerId, candidate, to_string(mlineindex));
    } else if (RECEIVER == user_data->peerDirection) {
        videoPipelineHandlerPtr->send_webrtc_video_receiver_ice(user_data->peerId, candidate, to_string(mlineindex));
    } else {
        GST_ERROR("Unknown sender/receiver type!");
    }
}

void on_incoming_stream_video(GstElement *webrtc, GstPad *pad, WebRtcPeer *webRtcVideoPeer) {
    GST_INFO("Triggered on_incoming_stream ");
    if (GST_PAD_DIRECTION (pad) != GST_PAD_SRC) {
        GST_ERROR("Incorrect pad direction");
        return;
    }

    GstCaps *caps;
    const gchar *name;
    gchar *dynamic_pad_name;
    GstElement *rtpvp8depay;
    dynamic_pad_name = gst_pad_get_name (pad);
    if (!gst_pad_has_current_caps(pad)) {
        GST_ERROR("Pad '%s' has no caps, can't do anything, ignoring",
                  GST_PAD_NAME(pad));
        return;
    }

    caps = gst_pad_get_current_caps(pad);
    name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
    GST_INFO("on_incoming_stream caps name %s ", name);
    gchar *tmp;
    tmp = g_strdup_printf("rtpvp8depay_send-%s", webRtcVideoPeer->peerId.c_str());
    VideoPipelineHandlerPtr videoPipelineHandlerPtr = push2talkUtils::fetch_video_pipelinehandler_by_key(
            webRtcVideoPeer->channelId);
    rtpvp8depay = gst_bin_get_by_name(GST_BIN (videoPipelineHandlerPtr->pipeline), tmp);
    g_free(tmp);
    if (gst_element_link_pads(webrtc, dynamic_pad_name, rtpvp8depay, "sink")) {
        GST_INFO("webrtc pad %s linked to rtpvp8depay_send", dynamic_pad_name);
        gst_object_unref(rtpvp8depay);
        g_free(dynamic_pad_name);
        //videoPipelineHandlerPtr->create_receivers_for_video();
        return;
    }
    return;
}

void launch_pipeline_video(std::string channelId);

gboolean VideoPipelineHandler::start_pipeline() {
    GST_INFO("starting video pipeline for channel %s ", channelId.c_str());
    std::thread th(launch_pipeline_video, channelId);
    th.detach();
    return TRUE;
}

void launch_pipeline_video(std::string channelId) {

    GST_INFO("Creating video pipeline channelId %s ", channelId.c_str());

    VideoPipelineHandlerPtr pipelineHandlerPtr = push2talkUtils::fetch_video_pipelinehandler_by_key(channelId);
    if (!pipelineHandlerPtr) {
        GST_INFO("ERROR - launch_new_pipeline: No matching video pipeline handler found for channel %s ",
                 channelId.c_str());
        return;
    }

    GstBus *bus;
    GstElement *rtpvp8depay, *watchdog, *videotee;

    /* Create Elements */
    gchar *tmp = g_strdup_printf("video-pipeline");
    pipelineHandlerPtr->pipeline = gst_pipeline_new(tmp);
    g_free(tmp);
    tmp = g_strdup_printf("webrtcbin_send-%s", pipelineHandlerPtr->senderPeerId.c_str());
    pipelineHandlerPtr->senderPeer->webrtcElement = gst_element_factory_make("webrtcbin", tmp);
    g_free(tmp);
    tmp = g_strdup_printf("rtpvp8depay_send-%s", pipelineHandlerPtr->senderPeerId.c_str());
    rtpvp8depay = gst_element_factory_make("rtpvp8depay", tmp);
    g_free(tmp);
    tmp = g_strdup_printf("watchdog_send-%s", pipelineHandlerPtr->senderPeerId.c_str());
    watchdog = gst_element_factory_make("watchdog", tmp);
    g_free(tmp);
    tmp = g_strdup_printf("videotee-%s", pipelineHandlerPtr->senderPeerId.c_str());
    videotee = gst_element_factory_make("tee", tmp);
    g_free(tmp);

    if (!pipelineHandlerPtr->pipeline || !rtpvp8depay || !watchdog || !videotee) {
        GST_ERROR("launch_new_pipeline: Cannot create elements for video %s ", "base pipeline");
        return;
    }

    /* Add Elements to pipeline and set properties */
    gst_bin_add_many(GST_BIN(pipelineHandlerPtr->pipeline), pipelineHandlerPtr->senderPeer->webrtcElement, rtpvp8depay,
                     videotee, NULL);
    g_object_set(watchdog, "timeout", 15000, NULL);
    g_object_set(videotee, "allow-not-linked", TRUE, NULL);

    if (!gst_element_link_many(rtpvp8depay, videotee, NULL)) {
        GST_ERROR("add_webrtc_video_receiver: Error linking rtpvp8depay to videotee for peer %s ",
                  pipelineHandlerPtr->senderPeerId.c_str());
        return;
    }

    g_assert_nonnull (pipelineHandlerPtr->senderPeer->webrtcElement);
    g_signal_connect (pipelineHandlerPtr->senderPeer->webrtcElement, "on-ice-candidate",
                      G_CALLBACK(send_ice_candidate_message_video), pipelineHandlerPtr->senderPeer.get());
    /* Incoming streams will be exposed via this signal */
    g_signal_connect (pipelineHandlerPtr->senderPeer->webrtcElement, "pad-added", G_CALLBACK(on_incoming_stream_video),
                      pipelineHandlerPtr->senderPeer.get());

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipelineHandlerPtr->pipeline));
    gst_bus_add_watch(bus, pipeline_bus_callback_video, pipelineHandlerPtr.get());
    gst_object_unref(GST_OBJECT(bus));

    //Add receivers
    GST_INFO("Creating receivers for sender peer %s ", pipelineHandlerPtr->senderPeerId.c_str());
    pipelineHandlerPtr->create_receivers_for_video();

    GST_INFO("Starting video pipeline channel %s sender peer %s ", pipelineHandlerPtr->channelId.c_str(),
             pipelineHandlerPtr->senderPeerId.c_str());
    GstStateChangeReturn return_val;
    return_val = gst_element_set_state(GST_ELEMENT (pipelineHandlerPtr->pipeline), GST_STATE_PLAYING);
    if (return_val == GST_STATE_CHANGE_FAILURE)
        goto err;

    //Attach received sdp info
    pipelineHandlerPtr->apply_webrtc_video_sender_sdp(pipelineHandlerPtr->senderPeerId,
                                                      pipelineHandlerPtr->senderPeer->sdp,
                                                      pipelineHandlerPtr->senderPeer->sdp_type);

    pipelineHandlerPtr->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(pipelineHandlerPtr->loop);
    GST_INFO("Main Loop Stopping for video pipeline channel %s sender peer %s ", pipelineHandlerPtr->channelId.c_str(),
             pipelineHandlerPtr->senderPeerId.c_str());
    g_main_loop_unref(pipelineHandlerPtr->loop);
    GST_INFO("Main Loop Stopped for video pipeline channel %s sender peer %s ", pipelineHandlerPtr->channelId.c_str(),
             pipelineHandlerPtr->senderPeerId.c_str());
    return;

    err:
    GST_ERROR("launch_new_pipeline: State change failure for sender video peer %s ",
              pipelineHandlerPtr->senderPeerId.c_str());
}

/* Answer created by our pipeline, to be sent to the peer */
void on_answer_created_video(GstPromise *promise, WebRtcPeer *webRtcVideoPeer) {
    GstWebRTCSessionDescription *answer;
    const GstStructure *reply;

    g_assert_cmpint (gst_promise_wait(promise), ==, GST_PROMISE_RESULT_REPLIED);
    reply = gst_promise_get_reply(promise);
    gst_structure_get(reply, "answer",
                      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);
    gst_promise_unref(promise);

    promise = gst_promise_new();
    g_assert_nonnull (webRtcVideoPeer->webrtcElement);
    g_signal_emit_by_name(webRtcVideoPeer->webrtcElement, "set-local-description", answer, promise);
    gst_promise_interrupt(promise);
    gst_promise_unref(promise);

    /* Send answer to peer */
    gchar *text;
    text = gst_sdp_message_as_text(answer->sdp);
    push2talkUtils::fetch_video_pipelinehandler_by_key(webRtcVideoPeer->channelId)->send_webrtc_video_sender_sdp(
            webRtcVideoPeer->peerId, text, "answer");
    gst_webrtc_session_description_free(answer);
}

void VideoPipelineHandler::apply_webrtc_video_sender_sdp(std::string peerId, std::string sdp, std::string type) {
    if (peerId.compare(senderPeerId) == 0) {
        GST_INFO("Received SDP type %s ", type.c_str());
        g_assert_cmpstr (type.c_str(), ==, "offer");
        int ret;
        GstSDPMessage *sdpMsg;
        const gchar *text;
        GstWebRTCSessionDescription *offer;
        text = sdp.c_str();
        GST_DEBUG("Received offer:\n%s", text);

        ret = gst_sdp_message_new(&sdpMsg);
        g_assert_cmphex (ret, ==, GST_SDP_OK);

        ret = gst_sdp_message_parse_buffer((guint8 *) text, strlen(text), sdpMsg);
        g_assert_cmphex (ret, ==, GST_SDP_OK);

        offer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_OFFER, sdpMsg);
        g_assert_nonnull (offer);

        /* Set remote description on our pipeline */
        {
            GstPromise *promise = gst_promise_new();
            g_signal_emit_by_name(senderPeer->webrtcElement, "set-remote-description", offer,
                                  promise);
            gst_promise_interrupt(promise);
            gst_promise_unref(promise);

            /* Create an answer that we will send back to the peer */
            promise = gst_promise_new_with_change_func((GstPromiseChangeFunc) on_answer_created_video,
                                                       (gpointer) senderPeer.get(), NULL);
            g_signal_emit_by_name(senderPeer->webrtcElement, "create-answer", NULL, promise);
        }
    } else {
        GST_ERROR("Other receiver peers cannot apply offer as sender! %s ", senderPeerId.c_str());
    }
}

void VideoPipelineHandler::apply_webrtc_video_receiver_sdp(std::string peerId, std::string sdp, std::string type) {
    WebRtcPeerPtr receiverPeer = fetch_video_reciever_by_peerid(peerId);
    if (receiverPeer) {
        int ret;
        GstSDPMessage *sdpMessage;
        const gchar *text, *sdptype;
        GstWebRTCSessionDescription *answer;

        sdptype = type.c_str();
        text = sdp.c_str();
        /* In this example, we always create the offer and receive one answer.
         * See tests/examples/webrtcbidirectional.c in gst-plugins-bad for how to
         * handle offers from peers and reply with answers using webrtcbin. */
        if (type.compare("answer") != 0) {
            GST_ERROR("Received non answer type with sdp for receiver peer %s:\n%s",
                      peerId.c_str(),
                      sdp.c_str());
            remove_webrtc_video_peer(peerId);
            return;
        }
        g_assert_cmpstr (sdptype, ==, "answer");

        GST_INFO("Received answer for receiver peer %s:\n%s", peerId.c_str(), sdp.c_str());

        ret = gst_sdp_message_new(&sdpMessage);
        g_assert_cmphex (ret, ==, GST_SDP_OK);

        ret = gst_sdp_message_parse_buffer((guint8 *) text, strlen(text), sdpMessage);
        if (ret != GST_SDP_OK) {
            GST_ERROR("Invalid SDP for receiver peer %s:\n%s", peerId.c_str(), sdp.c_str());
            remove_webrtc_video_peer(peerId);
            return;
        }
        g_assert_cmphex (ret, ==, GST_SDP_OK);

        answer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER,
                                                    sdpMessage);
        g_assert_nonnull (answer);

        /* Set remote description on our pipeline */
        {
            GstPromise *promise = gst_promise_new();
            g_signal_emit_by_name(receiverPeer->webrtcElement, "set-remote-description", answer,
                                  promise);
            gst_promise_interrupt(promise);
            gst_promise_unref(promise);
        }
        GST_INFO("apply_webrtc_audio_receiver_sdp completed for receiver peer %s ", peerId.c_str());
    } else {
        GST_ERROR("No receiver peer %s, cannot apply offer!", peerId.c_str());
    }
}

void VideoPipelineHandler::apply_webrtc_video_sender_ice(std::string peerId, std::string ice, std::string mLineIndex) {
    if (peerId.compare(senderPeerId) == 0) {
        int mlineindex = std::stoi(mLineIndex);
        while ((senderPeer->webrtcElement == NULL) || !(G_TYPE_CHECK_INSTANCE(senderPeer->webrtcElement))) {
            GST_INFO("Waiting for ICE to be applied for sender peer %s ", senderPeerId.c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        g_signal_emit_by_name(senderPeer->webrtcElement, "add-ice-candidate", mlineindex,
                              ice.c_str());
    } else {
        GST_ERROR("Other receiver peers cannot apply ICE as sender!");
    }
}

void
VideoPipelineHandler::apply_webrtc_video_receiver_ice(std::string peerId, std::string ice, std::string mLineIndex) {
    WebRtcPeerPtr receiverPeer = fetch_video_reciever_by_peerid(peerId);
    if (receiverPeer) {
        const gchar *candidateMsg;
        gint sdpmlineindex;

        candidateMsg = ice.c_str();
        sdpmlineindex = stoi(mLineIndex);
        GST_INFO("Received ice for receiver peer %s:\n sdpmlineindex: %d \n %s", peerId.c_str(),
                 sdpmlineindex, candidateMsg);

        /* Add ice candidateMsg sent by remote peer */
        g_signal_emit_by_name(receiverPeer->webrtcElement, "add-ice-candidate", sdpmlineindex,
                              candidateMsg);
    } else {
        GST_ERROR("Other receiver peers cannot apply offer!");
    }
}

void VideoPipelineHandler::send_webrtc_video_sender_sdp(std::string peerId, std::string sdp, std::string type) {
    PeerMessageRequest peerMessageRequest;
    peerMessageRequest.mutable_sdpmessage()->set_sdp(sdp);
    peerMessageRequest.mutable_sdpmessage()->set_type(type);
    peerMessageRequest.mutable_sdpmessage()->set_direction(SdpMessage_Direction_SENDER);
    peerMessageRequest.mutable_sdpmessage()->set_mediatype(SdpMessage_MediaType_VIDEO);
    peerMessageRequest.mutable_sdpmessage()->set_endpoint(SdpMessage_Endpoint_SERVER);
    peerMessageRequest.set_peerid(peerId);
    peerMessageRequest.set_channelid(channelId);
    push2talkUtils::pushToTalkServiceClientPtr->sendPeerMessage(peerMessageRequest);
}

void VideoPipelineHandler::send_webrtc_video_receiver_sdp(std::string peerId, std::string sdp, std::string type) {
    PeerMessageRequest peerMessageRequest;
    peerMessageRequest.mutable_sdpmessage()->set_sdp(sdp);
    peerMessageRequest.mutable_sdpmessage()->set_type(type);
    peerMessageRequest.mutable_sdpmessage()->set_direction(SdpMessage_Direction_RECEIVER);
    peerMessageRequest.mutable_sdpmessage()->set_mediatype(SdpMessage_MediaType_VIDEO);
    peerMessageRequest.mutable_sdpmessage()->set_endpoint(SdpMessage_Endpoint_SERVER);
    peerMessageRequest.set_peerid(peerId);
    peerMessageRequest.set_channelid(channelId);
    push2talkUtils::pushToTalkServiceClientPtr->sendPeerMessage(peerMessageRequest);
}

void VideoPipelineHandler::send_webrtc_video_sender_ice(std::string peerId, std::string ice, std::string mLineIndex) {
    PeerMessageRequest peerMessageRequest;
    peerMessageRequest.mutable_icemessage()->set_ice(ice);
    peerMessageRequest.mutable_icemessage()->set_mlineindex(mLineIndex);
    peerMessageRequest.mutable_icemessage()->set_direction(IceMessage_Direction_SENDER);
    peerMessageRequest.mutable_icemessage()->set_mediatype(IceMessage_MediaType_VIDEO);
    peerMessageRequest.mutable_icemessage()->set_endpoint(IceMessage_Endpoint_SERVER);
    peerMessageRequest.set_peerid(peerId);
    peerMessageRequest.set_channelid(channelId);
    push2talkUtils::pushToTalkServiceClientPtr->sendPeerMessage(peerMessageRequest);
}

void VideoPipelineHandler::send_webrtc_video_receiver_ice(std::string peerId, std::string ice, std::string mLineIndex) {
    PeerMessageRequest peerMessageRequest;
    peerMessageRequest.mutable_icemessage()->set_ice(ice);
    peerMessageRequest.mutable_icemessage()->set_mlineindex(mLineIndex);
    peerMessageRequest.mutable_icemessage()->set_direction(IceMessage_Direction_RECEIVER);
    peerMessageRequest.mutable_icemessage()->set_mediatype(IceMessage_MediaType_VIDEO);
    peerMessageRequest.mutable_icemessage()->set_endpoint(IceMessage_Endpoint_SERVER);
    peerMessageRequest.set_peerid(peerId);
    peerMessageRequest.set_channelid(channelId);
    push2talkUtils::pushToTalkServiceClientPtr->sendPeerMessage(peerMessageRequest);
}

void VideoPipelineHandler::remove_receiver_peer_from_pipeline(std::string peerId, bool remove_from_list) {
    gchar *tmp;
    GstPad *srcpad, *sinkpad;
    GstElement *webrtc, *rtpvp8pay, *queue, *tee;

    GST_INFO("remove receiver for peer %s ", peerId.c_str());

    tmp = g_strdup_printf("webrtcbin_recv-%s", peerId.c_str());
    webrtc = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    g_free(tmp);

    if (webrtc) {
        GST_INFO("Removing existing webrtcbin for remote peer %s ", peerId.c_str());
        gst_element_set_state(webrtc, GST_STATE_NULL);
        gst_bin_remove(GST_BIN (pipeline), webrtc);
        gst_object_unref(webrtc);
    } else {
        GST_ERROR("remove_peer_from_pipeline for remote peer %s - No existing webrtcbin found", peerId.c_str());
        remove_video_reciever_by_peerid(peerId);
        return;
    }

    tmp = g_strdup_printf("rtphvp8pay_recv-%s", peerId.c_str());
    rtpvp8pay = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    g_free(tmp);
    if (rtpvp8pay) {
        gst_element_set_state(rtpvp8pay, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(pipeline), rtpvp8pay);
        gst_object_unref(rtpvp8pay);
    }

    tmp = g_strdup_printf("queue_recv-%s", peerId.c_str());
    queue = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    g_free(tmp);

    if (queue) {
        gst_element_set_state(queue, GST_STATE_NULL);

        sinkpad = gst_element_get_static_pad(queue, "sink");
        g_assert_nonnull (sinkpad);
        srcpad = gst_pad_get_peer(sinkpad);
        g_assert_nonnull (srcpad);
        gst_object_unref(sinkpad);

        gst_bin_remove(GST_BIN (pipeline), queue);
        gst_object_unref(queue);

        tee = gst_bin_get_by_name(GST_BIN (pipeline), "videotee");
        if (tee) {
            g_assert_nonnull (tee);
            gst_element_release_request_pad(tee, srcpad);
            gst_object_unref(srcpad);
            gst_object_unref(tee);
        }
    }
    GST_INFO("Removed webrtcbin peer for remote peer : %s", peerId.c_str());
    if (remove_from_list) {
        remove_video_reciever_by_peerid(peerId);
    }
}

void VideoPipelineHandler::remove_sender_peer_close_pipeline() {
    if (pipeline) {
        try {
            for (auto elem : peerVideoReceivers) {
                WebRtcPeer webRtcPeerPtr = *(elem.second);
                remove_receiver_peer_from_pipeline(webRtcPeerPtr.peerId, false);
            }
            GST_INFO("Clearing all receiver peers list");
            peerVideoReceivers.clear();
        } catch (std::exception exception1) {
            GST_ERROR("stop_and_clear_allpeers_from_map ::  Exception Thrown %s ", exception1.what());
        }

        std::future<bool> futureStateNull = std::async(std::launch::async, [](string channel_id) {
            VideoPipelineHandlerPtr pipelineHandlerPtr = push2talkUtils::fetch_video_pipelinehandler_by_key(channel_id);
            if (!pipelineHandlerPtr) {
                GST_ERROR("futureStateNull Async: No matching video handler found for sender channel %s ",
                          channel_id.c_str());
                return false;
            }
            GST_INFO("futureStateNull Async: Setting state for video channel %s as NULL ", channel_id.c_str());
            gst_element_set_state(GST_ELEMENT(pipelineHandlerPtr->pipeline), GST_STATE_NULL);
            return true;
        }, channelId);
        if (std::future_status::ready == futureStateNull.wait_for(std::chrono::milliseconds(150))) {
            GST_INFO("Nullified pipeline for channel %s with return %d ", channelId.c_str(), futureStateNull.get());
        } else {
            GST_INFO("Nullifying pipeline timeout for channel %s ", channelId.c_str());
        }
    }
    if (loop) {
        GST_INFO("Stopping main loop for peer id %s ", senderPeerId.c_str());
        g_main_quit(loop);
    }
    push2talkUtils::remove_video_pipeline_handler(channelId);
}

void VideoPipelineHandler::remove_webrtc_video_peer(std::string peerId) {
    if (peerId.compare(senderPeerId) == 0) {
        remove_sender_peer_close_pipeline();
    } else {
        remove_receiver_peer_from_pipeline(peerId, true);
    }
}

void send_sdp_offer_video(GstWebRTCSessionDescription *offer, WebRtcPeer *webRtcAudioPeer) {
    gchar *text;

    text = gst_sdp_message_as_text(offer->sdp);
    GST_INFO("Sending offer for peer: %s \n %s", webRtcAudioPeer->peerId.c_str(), text);

    push2talkUtils::fetch_video_pipelinehandler_by_key(webRtcAudioPeer->channelId)->send_webrtc_video_receiver_sdp(
            webRtcAudioPeer->peerId, text, "offer");
    g_free(text);
}

/* Offer created by our audio pipeline, to be sent to the peer */
void on_offer_created_video(GstPromise *promise, gpointer user_data) {
    GstWebRTCSessionDescription *offer = NULL;
    const GstStructure *reply;
    WebRtcPeer *webRtcVideoPeer = static_cast<WebRtcPeer *>(user_data);

    g_assert_cmphex (gst_promise_wait(promise), ==, GST_PROMISE_RESULT_REPLIED);
    reply = gst_promise_get_reply(promise);
    gst_structure_get(reply, "offer",
                      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);
    gst_promise_unref(promise);
    promise = gst_promise_new();
    g_signal_emit_by_name(webRtcVideoPeer->webrtcElement, "set-local-description", offer, promise);
    gst_promise_interrupt(promise);
    gst_promise_unref(promise);

    /* Send offer to peer */
    send_sdp_offer_video(offer, webRtcVideoPeer);
    gst_webrtc_session_description_free(offer);
}

void on_negotiation_needed_video(GstElement *element, WebRtcPeer *user_data) {
    GstPromise *promise;
    promise = gst_promise_new_with_change_func(on_offer_created_video, user_data, NULL);
    g_signal_emit_by_name(user_data->webrtcElement, "create-offer", NULL, promise);
}

void VideoPipelineHandler::create_receivers_for_video() {
    GST_INFO("Creating video receivers %lu for sender peer %s ", peerVideoReceivers.size(), senderPeerId.c_str());
    for (std::pair<std::string, WebRtcPeerPtr> receiver : peerVideoReceivers) {
        WebRtcPeerPtr webRtcPeerPtr = receiver.second;
        GST_INFO("Creating webrtc recv peer for peer id %s ", webRtcPeerPtr->peerId.c_str());

        GstWebRTCRTPTransceiver *trans;
        GArray *transceivers;
        int ret;
        gchar *tmp;
        GstElement *tee, *queue, *rtpvp8pay;
        GstCaps *caps;
        GstPad *srcpad, *sinkpad;
        //Create queue
        tmp = g_strdup_printf("queue_recv-%s", webRtcPeerPtr->peerId.c_str());
        queue = gst_element_factory_make("queue", tmp);
        g_object_set(queue, "leaky", 2, NULL);
        g_free(tmp);

        //Create rtph264pay with caps
        tmp = g_strdup_printf("rtphvp8pay_recv-%s", webRtcPeerPtr->peerId.c_str());
        rtpvp8pay = gst_element_factory_make("rtpvp8pay", tmp);
        g_object_set(rtpvp8pay, "config-interval", -1, NULL);
        g_object_set(rtpvp8pay, "pt", 96, NULL);
        g_free(tmp);

        srcpad = gst_element_get_static_pad(rtpvp8pay, "src");
        caps = gst_caps_from_string(RTP_CAPS_VP8);
        gst_pad_set_caps(srcpad, caps);
        gst_caps_unref(caps);
        gst_object_unref(srcpad);

        //Create webrtcbin
        tmp = g_strdup_printf("webrtcbin_recv-%s", webRtcPeerPtr->peerId.c_str());
        webRtcPeerPtr->webrtcElement = gst_element_factory_make("webrtcbin", tmp);
        g_object_set(webRtcPeerPtr->webrtcElement, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE, NULL);
        g_free(tmp);

        //Add elements to pipeline
        gst_bin_add_many(GST_BIN (pipeline), queue, rtpvp8pay, webRtcPeerPtr->webrtcElement, NULL);

        //Link queue -> rtpvp8pay
        srcpad = gst_element_get_static_pad(queue, "src");
        g_assert_nonnull (srcpad);
        sinkpad = gst_element_get_static_pad(rtpvp8pay, "sink");
        g_assert_nonnull (sinkpad);
        ret = gst_pad_link(srcpad, sinkpad);
        g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
        gst_object_unref(srcpad);
        gst_object_unref(sinkpad);

        //Link rtph264depay -> webrtcbin
        srcpad = gst_element_get_static_pad(rtpvp8pay, "src");
        g_assert_nonnull (srcpad);
        sinkpad = gst_element_get_request_pad(webRtcPeerPtr->webrtcElement, "sink_%u");
        g_assert_nonnull (sinkpad);
        ret = gst_pad_link(srcpad, sinkpad);
        g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
        gst_object_unref(srcpad);
        gst_object_unref(sinkpad);

        //Link videotee -> queue
        tmp = g_strdup_printf("videotee-%s", senderPeerId.c_str());
        tee = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
        g_free(tmp);
        g_assert_nonnull (tee);
        srcpad = gst_element_get_request_pad(tee, "src_%u");
        g_assert_nonnull (srcpad);
        gst_object_unref(tee);
        sinkpad = gst_element_get_static_pad(queue, "sink");
        g_assert_nonnull (sinkpad);
        ret = gst_pad_link(srcpad, sinkpad);
        g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
        gst_object_unref(srcpad);
        gst_object_unref(sinkpad);

        //Change webrtcbin to send only
        g_signal_emit_by_name(webRtcPeerPtr->webrtcElement, "get-transceivers", &transceivers);
        if (transceivers) {
            GST_INFO("Changing webrtcbin for peer %s to sendonly. Number of transceivers(%d) ",
                     webRtcPeerPtr->peerId.c_str(),
                     transceivers->len);
            g_assert_true(transceivers->len > 0);
            trans = g_array_index (transceivers, GstWebRTCRTPTransceiver *, 0);
            trans->direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
            g_array_unref(transceivers);
        }

        g_assert_nonnull (webRtcPeerPtr->webrtcElement);
        /* This is the gstwebrtc entry point where we create the offer and so on. It
         * will be called when the pipeline goes to PLAYING. */
        g_signal_connect (webRtcPeerPtr->webrtcElement, "on-negotiation-needed",
                          G_CALLBACK(on_negotiation_needed_video), webRtcPeerPtr.get());
        /* We need to transmit this ICE candidate to the browser via the websockets
         * signalling server. Incoming ice candidates from the browser need to be
         * added by us too, see on_server_message() */
        g_signal_connect (webRtcPeerPtr->webrtcElement, "on-ice-candidate",
                          G_CALLBACK(send_ice_candidate_message_video), webRtcPeerPtr.get());

        /* Incoming streams will be exposed via this signal */
        //g_signal_connect (webrtcbin, "pad-added", G_CALLBACK(on_incoming_stream), pipeline);

        ret = gst_element_sync_state_with_parent(queue);
        g_assert_true (ret);
        ret = gst_element_sync_state_with_parent(rtpvp8pay);
        g_assert_true (ret);
        ret = gst_element_sync_state_with_parent(webRtcPeerPtr->webrtcElement);
        g_assert_true (ret);

        GST_INFO("Created webrtc bin for receiver peer %s", webRtcPeerPtr->peerId.c_str());
    }
}

WebRtcPeerPtr VideoPipelineHandler::fetch_video_reciever_by_peerid(std::string peerId) {
    std::lock_guard<std::mutex> lockGuard(push2talkUtils::peers_mutex);
    GST_INFO("Fetching receiver video peer with peerid %s ", peerId.c_str());
    auto it_peer = peerVideoReceivers.find(peerId);
    if (it_peer != peerVideoReceivers.end()) {
        return it_peer->second;
    } else {
        return NULL;
    }
}

void VideoPipelineHandler::remove_video_reciever_by_peerid(std::string peerId) {
    std::lock_guard<std::mutex> lockGuard(push2talkUtils::peers_mutex);
    auto it_peer = peerVideoReceivers.find(peerId);
    if (it_peer != peerVideoReceivers.end()) {
        peerVideoReceivers.erase(it_peer);
        GST_INFO("Deleted receiver video peer from map for peer %s", peerId.c_str());
    }
}

