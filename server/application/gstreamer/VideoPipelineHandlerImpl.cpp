//
// Created by chakra on 29-03-2020.
//

#define GST_USE_UNSTABLE_API

#include <thread>
#include <string>
#include <future> // std::async, std::future
#include <chrono>
#include <gst/webrtc/webrtc.h>
#include <gst/rtp/rtp.h>

#include "VideoPipelineHandler.hpp"
#include "GstMediaUtils.hpp"
#include "../grpc/GrpcService.hpp"
#include "../utils/Push2TalkUtils.hpp"
#include "WebRtcPeer.hpp"

GST_DEBUG_CATEGORY_EXTERN (push2talk_gst);
#define GST_CAT_DEFAULT push2talk_gst

#define STUN_SERVER " stun-server=stun://stun.l.google.com:19302"
#define RTP_CAPS_OPUS "application/x-rtp,media=audio,encoding-name=OPUS,payload=111"
#define RTP_CAPS_VP8 "application/x-rtp,media=video,encoding-name=VP8,payload=96"

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
    SdpMediaStatePtr mediaStatePtr = gstMediaUtils::load_media_state_in_sdp(sdp);
    if (!mediaStatePtr->valid) {
        GST_ERROR("Invalid SDP received for sender peer Id %s as %s ", senderPeerId.c_str(), senderPeer->sdp.c_str());
        release_video_pipeline(this);
        return;
    }
    audio_valid = mediaStatePtr->audio;
    video_valid = mediaStatePtr->video;

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
    char *capsName;
    gchar *dynamic_pad_name;
    dynamic_pad_name = gst_pad_get_name (pad);
    if (!gst_pad_has_current_caps(pad)) {
        GST_ERROR("Pad '%s' has no caps, can't do anything, ignoring",
                  GST_PAD_NAME(pad));
        return;
    }

    caps = gst_pad_get_current_caps(pad);
    capsName = gst_caps_to_string(caps);
    name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
    GST_INFO("on_incoming_stream caps name %s capsName %s ", name, capsName);
    VideoPipelineHandlerPtr videoPipelineHandlerPtr = push2talkUtils::fetch_video_pipelinehandler_by_key(
            webRtcVideoPeer->channelId);

    if (g_strrstr(capsName, "audio")) {
        gchar *tmp;
        GstElement *rtpopusdepay;
        tmp = g_strdup_printf("rtpopusdepay_send-%s", webRtcVideoPeer->peerId.c_str());
        rtpopusdepay = gst_bin_get_by_name(GST_BIN (videoPipelineHandlerPtr->pipeline), tmp);
        g_free(tmp);
        if (gst_element_link_pads(webrtc, dynamic_pad_name, rtpopusdepay, "sink")) {
            GST_INFO("webrtc pad %s linked to rtpopusdepay_send", dynamic_pad_name);
        }
        gst_object_unref(rtpopusdepay);
    } else if (g_strrstr(capsName, "video")) {
        gchar *tmp;
        GstElement *rtpvp8depay;
        webRtcVideoPeer->videoSrcPadName = dynamic_pad_name;
        tmp = g_strdup_printf("rtpvp8depay_send-%s", webRtcVideoPeer->peerId.c_str());
        rtpvp8depay = gst_bin_get_by_name(GST_BIN (videoPipelineHandlerPtr->pipeline), tmp);
        g_free(tmp);
        if (gst_element_link_pads(webrtc, dynamic_pad_name, rtpvp8depay, "sink")) {
            GST_INFO("webrtc pad %s linked to rtpvp8depay_send", dynamic_pad_name);
        }
        gst_object_unref(rtpvp8depay);
    }
    g_free(dynamic_pad_name);
    g_free(capsName);
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
    /* Create Elements */
    gchar *tmp = g_strdup_printf("video-pipeline");
    pipelineHandlerPtr->pipeline = gst_pipeline_new(tmp);
    g_free(tmp);
    tmp = g_strdup_printf("webrtcbin_send-%s", pipelineHandlerPtr->senderPeerId.c_str());
    pipelineHandlerPtr->senderPeer->webrtcElement = gst_element_factory_make("webrtcbin", tmp);
    g_object_set(pipelineHandlerPtr->senderPeer->webrtcElement, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE,
                 NULL);
    g_free(tmp);
    gst_bin_add_many(GST_BIN(pipelineHandlerPtr->pipeline), pipelineHandlerPtr->senderPeer->webrtcElement, NULL);

    if (pipelineHandlerPtr->video_valid) {
        GstElement *rtpvp8depay, *watchdog_video, *videotee;
        tmp = g_strdup_printf("rtpvp8depay_send-%s", pipelineHandlerPtr->senderPeerId.c_str());
        rtpvp8depay = gst_element_factory_make("rtpvp8depay", tmp);
        g_free(tmp);
        tmp = g_strdup_printf("watchdog_video_send-%s", pipelineHandlerPtr->senderPeerId.c_str());
        watchdog_video = gst_element_factory_make("watchdog", tmp);
        g_free(tmp);
        tmp = g_strdup_printf("videotee-%s", pipelineHandlerPtr->senderPeerId.c_str());
        videotee = gst_element_factory_make("tee", tmp);
        g_free(tmp);

        if (!pipelineHandlerPtr->pipeline || !rtpvp8depay || !watchdog_video || !videotee) {
            GST_ERROR("launch_new_pipeline: Cannot create elements for video %s ", "base pipeline");
            return;
        }

        /* Add Elements to pipeline and set properties */
        gst_bin_add_many(GST_BIN(pipelineHandlerPtr->pipeline), rtpvp8depay, videotee, NULL);
        g_object_set(videotee, "allow-not-linked", TRUE, NULL);

        if (pipelineHandlerPtr->apply_watchdog) {
            gst_bin_add_many(GST_BIN(pipelineHandlerPtr->pipeline), watchdog_video, NULL);
            g_object_set(watchdog_video, "timeout", WATCHDOG_MILLI_SECS, NULL);
            if (!gst_element_link_many(rtpvp8depay, watchdog_video, videotee, NULL)) {
                GST_ERROR("add_webrtc_video_receiver: Error linking rtpvp8depay to watchdog to videotee for peer %s ",
                          pipelineHandlerPtr->senderPeerId.c_str());
                return;
            }
        } else {
            if (!gst_element_link_many(rtpvp8depay, videotee, NULL)) {
                GST_ERROR("add_webrtc_video_receiver: Error linking rtpvp8depay to videotee for peer %s ",
                          pipelineHandlerPtr->senderPeerId.c_str());
                return;
            }
        }
    }

    if (pipelineHandlerPtr->audio_valid) {
        GstElement *opusdepay, *watchdog_audio, *opusdec, *opusenc, *audiotee;
        tmp = g_strdup_printf("rtpopusdepay_send-%s", pipelineHandlerPtr->senderPeerId.c_str());
        opusdepay = gst_element_factory_make("rtpopusdepay", tmp);
        g_free(tmp);
        tmp = g_strdup_printf("watchdog_audio_send-%s", pipelineHandlerPtr->senderPeerId.c_str());
        watchdog_audio = gst_element_factory_make("watchdog", tmp);
        g_free(tmp);
        tmp = g_strdup_printf("opusdec_audio_send-%s", pipelineHandlerPtr->senderPeerId.c_str());
        opusdec = gst_element_factory_make("opusdec", tmp);
        g_free(tmp);
        tmp = g_strdup_printf("opusenc_audio_send-%s", pipelineHandlerPtr->senderPeerId.c_str());
        opusenc = gst_element_factory_make("opusenc", tmp);
        g_free(tmp);
        tmp = g_strdup_printf("audiotee-%s", pipelineHandlerPtr->senderPeerId.c_str());
        audiotee = gst_element_factory_make("tee", tmp);
        g_free(tmp);

        if (!pipelineHandlerPtr->pipeline || !opusdepay || !watchdog_audio || !opusdec || !opusenc || !audiotee) {
            GST_ERROR("launch_new_pipeline: Cannot create elements for audio %s ", "base pipeline");
            return;
        }

        /* Add Elements to pipeline and set properties */
        gst_bin_add_many(GST_BIN(pipelineHandlerPtr->pipeline), opusdepay, opusdec, opusenc, audiotee, NULL);
        g_object_set(audiotee, "allow-not-linked", TRUE, NULL);

        if (pipelineHandlerPtr->apply_watchdog) {
            gst_bin_add_many(GST_BIN(pipelineHandlerPtr->pipeline), watchdog_audio, NULL);
            g_object_set(watchdog_audio, "timeout", WATCHDOG_MILLI_SECS, NULL);
            if (!gst_element_link_many(opusdepay, watchdog_audio, opusdec, opusenc, audiotee, NULL)) {
                GST_ERROR(
                        "add_webrtc_video_receiver: Error linking rtpopusdepay to watchdog to opusdec/enc to audiotee for peer %s ",
                        pipelineHandlerPtr->senderPeerId.c_str());
                return;
            }
        } else {
            if (!gst_element_link_many(opusdepay, opusdec, opusenc, audiotee, NULL)) {
                GST_ERROR(
                        "add_webrtc_video_receiver: Error linking rtpopusdepay to opusdec/enc to audiotee for peer %s ",
                        pipelineHandlerPtr->senderPeerId.c_str());
                return;
            }
        }
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

gboolean send_key_frame_timer_callback(gpointer user_data) {
    GString *val = (GString *) user_data;
    VideoPipelineHandlerPtr videoPipelineHandlerPtr = push2talkUtils::fetch_video_pipelinehandler_by_key(val->str);
    if (videoPipelineHandlerPtr) {
        videoPipelineHandlerPtr->send_key_frame_request_to_sender();
        return TRUE;
    }
    GST_INFO("Video pipeline handler invalid for channel %s. Might be stopped ", val->str);
    g_string_free(val, TRUE);
    return FALSE;
}

void VideoPipelineHandler::send_key_frame_request_to_sender() {
    GST_DEBUG("Sending key frame request for sender peer %s ", senderPeerId.c_str());
    if (pipeline) {
        GstPad *videosrcpad;
        videosrcpad = gst_element_get_static_pad(senderPeer->webrtcElement, senderPeer->videoSrcPadName.c_str());
        if (videosrcpad) {
            gst_pad_send_event(videosrcpad, gst_event_new_custom(GST_EVENT_CUSTOM_UPSTREAM,
                                                                 gst_structure_new("GstForceKeyUnit", "all-headers",
                                                                                   G_TYPE_BOOLEAN, TRUE, NULL)));
        } else {
            GST_ERROR("Sender webrtcbin video src pad %s not exists for sender peer %s ",
                      senderPeer->videoSrcPadName.c_str(), senderPeerId.c_str());
        }
    } else {
        GST_ERROR("Pipeline not exists for sender peer %s ", senderPeerId.c_str());
    }
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
    GstElement *webrtc, *rtpvp8pay, *rtpopuspay, *videoqueue, *audioqueue, *videotee, *audiotee;

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

    tmp = g_strdup_printf("rtpopuspay_recv-%s", peerId.c_str());
    rtpopuspay = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    g_free(tmp);
    if (rtpopuspay) {
        gst_element_set_state(rtpopuspay, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(pipeline), rtpopuspay);
        gst_object_unref(rtpopuspay);
    }

    tmp = g_strdup_printf("queue_video_recv-%s", peerId.c_str());
    videoqueue = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    g_free(tmp);

    if (videoqueue) {
        gst_element_set_state(videoqueue, GST_STATE_NULL);

        sinkpad = gst_element_get_static_pad(videoqueue, "sink");
        g_assert_nonnull (sinkpad);
        srcpad = gst_pad_get_peer(sinkpad);
        g_assert_nonnull (srcpad);
        gst_object_unref(sinkpad);

        gst_bin_remove(GST_BIN (pipeline), videoqueue);
        gst_object_unref(videoqueue);

        tmp = g_strdup_printf("videotee-%s", senderPeerId.c_str());
        videotee = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
        g_free(tmp);
        if (videotee) {
            g_assert_nonnull (videotee);
            gst_element_release_request_pad(videotee, srcpad);
            gst_object_unref(srcpad);
            gst_object_unref(videotee);
        }
    }

    tmp = g_strdup_printf("queue_audio_recv-%s", peerId.c_str());
    audioqueue = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    g_free(tmp);

    if (audioqueue) {
        gst_element_set_state(audioqueue, GST_STATE_NULL);

        sinkpad = gst_element_get_static_pad(audioqueue, "sink");
        g_assert_nonnull (sinkpad);
        srcpad = gst_pad_get_peer(sinkpad);
        g_assert_nonnull (srcpad);
        gst_object_unref(sinkpad);

        gst_bin_remove(GST_BIN (pipeline), audioqueue);
        gst_object_unref(audioqueue);

        tmp = g_strdup_printf("audiotee-%s", senderPeerId.c_str());
        audiotee = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
        g_free(tmp);
        if (audiotee) {
            g_assert_nonnull (audiotee);
            gst_element_release_request_pad(audiotee, srcpad);
            gst_object_unref(srcpad);
            gst_object_unref(audiotee);
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
        pipeline = NULL;
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

gboolean receiver_on_sending_rtcp_cb(GstElement *bin, guint sessid, GstBuffer *buffer, gpointer udata) {
    GST_DEBUG("Triggered on sending receiver RTCP");
}

gboolean receiver_on_receiving_rtcp_cb(GstElement *bin, guint sessid, GstBuffer *buffer, gpointer udata) {
    GST_INFO("Triggered on receiving receiver RTCP");
    gboolean result = gst_rtcp_buffer_validate(buffer);
    if (result) {
        GST_INFO("on_receiving_rtcp_cb: RTCP Buffer valid: %d ", result);
    }
}

GstPadProbeReturn rtcp_sink_buffer_pad_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
    GstBuffer *gstBuffer;
    gstBuffer = gst_pad_probe_info_get_buffer(info);
    gboolean result = gst_rtcp_buffer_validate(gstBuffer);
    if (result) {
        GST_DEBUG("Pad Probe: RTCP Buffer valid: %d ", result);
        GstRTCPBuffer rtcpBuffer = {NULL,};
        result = gst_rtcp_buffer_map(gstBuffer, GST_MAP_READ, &rtcpBuffer);
        if (result) {
            GST_DEBUG("RTCP Buffer valid: %d ", result);
            GstRTCPPacket packet;
            gboolean morePackets;
            morePackets = gst_rtcp_buffer_get_first_packet(&rtcpBuffer, &packet);
            while (morePackets) {
                GstRTCPType type;
                type = gst_rtcp_packet_get_type(&packet);
                switch (type) {
                    case GST_RTCP_TYPE_PSFB:
                    case GST_RTCP_TYPE_RTPFB:
                        GstRTCPFBType gstRtcpfbType;
                        gstRtcpfbType = gst_rtcp_packet_fb_get_type(&packet);
                        switch (gstRtcpfbType) {
                            case GST_RTCP_PSFB_TYPE_FIR:
                            case GST_RTCP_PSFB_TYPE_PLI:
                                GST_INFO("+++++++++++++++++++++++++++++++++++++++++PLI/FIR Type found");
                                break;
                            default:
                                GST_DEBUG ("Default /Unhandled FB Type: %d", gstRtcpfbType);
                                break;
                        }
                        break;
                    default:
                        GST_DEBUG ("Default /Unhandled RTCP Type: %d", type);
                }
                morePackets = gst_rtcp_packet_move_to_next(&packet);
            }
            gst_rtcp_buffer_unmap(&rtcpBuffer);
        }
    }
    return GST_PAD_PROBE_OK;
}

void on_new_ssrc_callback_receiver(GstElement *rtpbin, guint session, guint ssrc, gpointer udata) {
    GST_INFO("New SSRC created for session %d as %d ", session, ssrc);
    GstPad *rtcp_sink_pad;
    gchar *padName;
    padName = g_strdup_printf("recv_rtcp_sink_%u", session);
    rtcp_sink_pad = gst_element_get_static_pad(rtpbin, padName);
    if (rtcp_sink_pad) {
        GST_INFO("Pad Already Exists with name %s ", padName);
    } else {
        rtcp_sink_pad = gst_element_get_request_pad(rtpbin, padName);
    }
    g_free(padName);
    if (rtcp_sink_pad) {
        gst_pad_add_probe(rtcp_sink_pad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback) rtcp_sink_buffer_pad_probe_cb,
                          NULL, NULL);
        gst_object_unref(rtcp_sink_pad);
    }
}

void new_storage_callback_rtp_receiver(GstElement *rtpbin, GstElement storage, guint session, gpointer udata) {
    GST_INFO("New RTPBin Storage for session id %d ", session);
    GObject *sessionRef;
    g_signal_emit_by_name(rtpbin, "get-internal-session", session, &sessionRef);
    if (sessionRef) {
        GST_INFO("Internal Session found for RTPBin recv");
        g_signal_connect(sessionRef, "on-receiving-rtcp", G_CALLBACK(receiver_on_receiving_rtcp_cb),
                         udata);
        g_signal_connect(sessionRef, "on-sending-rtcp", G_CALLBACK(receiver_on_sending_rtcp_cb),
                         udata);
    }
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
        GstPad *srcpad, *sinkpad;

        //Create webrtcbin
        tmp = g_strdup_printf("webrtcbin_recv-%s", webRtcPeerPtr->peerId.c_str());
        webRtcPeerPtr->webrtcElement = gst_element_factory_make("webrtcbin", tmp);
        g_object_set(webRtcPeerPtr->webrtcElement, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE, NULL);
        g_free(tmp);

        gst_bin_add_many(GST_BIN (pipeline), webRtcPeerPtr->webrtcElement, NULL);

        g_assert_nonnull (webRtcPeerPtr->webrtcElement);
        /* This is the gstwebrtc entry point where we create the offer and so on. It
         * will be called when the pipeline goes to PLAYING. */
        g_signal_connect (webRtcPeerPtr->webrtcElement, "on-negotiation-needed",
                          G_CALLBACK(on_negotiation_needed_video), webRtcPeerPtr.get());
        /* Need to transmit this ICE candidate to remote peer via
         * signalling server. Incoming ice candidates from the peer need to be
         * added */
        g_signal_connect (webRtcPeerPtr->webrtcElement, "on-ice-candidate",
                          G_CALLBACK(send_ice_candidate_message_video), webRtcPeerPtr.get());

        /* Incoming streams will be exposed via this signal */
        //g_signal_connect (webrtcbin, "pad-added", G_CALLBACK(on_incoming_stream), pipeline);
        GstElement *rtpbin;
        rtpbin = gst_bin_get_by_name(GST_BIN(webRtcPeerPtr->webrtcElement), "rtpbin");
        if (rtpbin) {
            //g_signal_connect (rtpbin, "on-new-ssrc", G_CALLBACK(on_new_ssrc_callback_receiver), webRtcPeerPtr.get());
            g_signal_connect (rtpbin, "new-storage", G_CALLBACK(new_storage_callback_rtp_receiver),
                              webRtcPeerPtr.get());
            g_object_unref(rtpbin);
        }

        if (video_valid) {
            GstElement *videotee, *videoqueue, *rtpvp8pay;
            GstCaps *caps;

            //Create queue
            tmp = g_strdup_printf("queue_video_recv-%s", webRtcPeerPtr->peerId.c_str());
            videoqueue = gst_element_factory_make("queue", tmp);
            g_object_set(videoqueue, "leaky", 2, NULL);
            g_free(tmp);

            //Create rtph264pay with caps
            tmp = g_strdup_printf("rtphvp8pay_recv-%s", webRtcPeerPtr->peerId.c_str());
            rtpvp8pay = gst_element_factory_make("rtpvp8pay", tmp);
            //g_object_set(rtpvp8pay, "config-interval", -1, NULL);
            g_object_set(rtpvp8pay, "pt", 96, NULL);
            g_free(tmp);

            srcpad = gst_element_get_static_pad(rtpvp8pay, "src");
            caps = gst_caps_from_string(RTP_CAPS_VP8);
            gst_pad_set_caps(srcpad, caps);
            gst_caps_unref(caps);
            gst_object_unref(srcpad);

            //Add elements to pipeline
            gst_bin_add_many(GST_BIN (pipeline), videoqueue, rtpvp8pay, NULL);

            //Link queue -> rtpvp8pay
            srcpad = gst_element_get_static_pad(videoqueue, "src");
            g_assert_nonnull (srcpad);
            sinkpad = gst_element_get_static_pad(rtpvp8pay, "sink");
            g_assert_nonnull (sinkpad);
            ret = gst_pad_link(srcpad, sinkpad);
            g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
            gst_object_unref(srcpad);
            gst_object_unref(sinkpad);

            //Link rtpvp8pay -> webrtcbin
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
            videotee = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
            g_free(tmp);
            g_assert_nonnull (videotee);
            srcpad = gst_element_get_request_pad(videotee, "src_%u");
            g_assert_nonnull (srcpad);
            gst_object_unref(videotee);
            sinkpad = gst_element_get_static_pad(videoqueue, "sink");
            g_assert_nonnull (sinkpad);
            ret = gst_pad_link(srcpad, sinkpad);
            g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
            gst_object_unref(srcpad);
            gst_object_unref(sinkpad);

            ret = gst_element_sync_state_with_parent(videoqueue);
            g_assert_true (ret);
            ret = gst_element_sync_state_with_parent(rtpvp8pay);
            g_assert_true (ret);
        }

        if (audio_valid) {
            GstElement *audiotee, *audioqueue, *rtpopuspay;
            GstCaps *caps;

            //Create queue
            tmp = g_strdup_printf("queue_audio_recv-%s", webRtcPeerPtr->peerId.c_str());
            audioqueue = gst_element_factory_make("queue", tmp);
            g_object_set(audioqueue, "leaky", 2, NULL);
            g_free(tmp);

            //Create rtpopuspay with caps
            tmp = g_strdup_printf("rtpopuspay_recv-%s", webRtcPeerPtr->peerId.c_str());
            rtpopuspay = gst_element_factory_make("rtpopuspay", tmp);
            g_free(tmp);

            srcpad = gst_element_get_static_pad(rtpopuspay, "src");
            caps = gst_caps_from_string(RTP_CAPS_OPUS);
            gst_pad_set_caps(srcpad, caps);
            gst_caps_unref(caps);
            gst_object_unref(srcpad);

            //Add elements to pipeline
            gst_bin_add_many(GST_BIN (pipeline), audioqueue, rtpopuspay, NULL);

            //Link queue -> rtpopuspay
            srcpad = gst_element_get_static_pad(audioqueue, "src");
            g_assert_nonnull (srcpad);
            sinkpad = gst_element_get_static_pad(rtpopuspay, "sink");
            g_assert_nonnull (sinkpad);
            ret = gst_pad_link(srcpad, sinkpad);
            g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
            gst_object_unref(srcpad);
            gst_object_unref(sinkpad);

            //Link rtpopuspay -> webrtcbin
            srcpad = gst_element_get_static_pad(rtpopuspay, "src");
            g_assert_nonnull (srcpad);
            sinkpad = gst_element_get_request_pad(webRtcPeerPtr->webrtcElement, "sink_%u");
            g_assert_nonnull (sinkpad);
            ret = gst_pad_link(srcpad, sinkpad);
            g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
            gst_object_unref(srcpad);
            gst_object_unref(sinkpad);

            //Link audiotee -> queue
            tmp = g_strdup_printf("audiotee-%s", senderPeerId.c_str());
            audiotee = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
            g_free(tmp);
            g_assert_nonnull (audiotee);
            srcpad = gst_element_get_request_pad(audiotee, "src_%u");
            g_assert_nonnull (srcpad);
            gst_object_unref(audiotee);
            sinkpad = gst_element_get_static_pad(audioqueue, "sink");
            g_assert_nonnull (sinkpad);
            ret = gst_pad_link(srcpad, sinkpad);
            g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
            gst_object_unref(srcpad);
            gst_object_unref(sinkpad);

            ret = gst_element_sync_state_with_parent(audioqueue);
            g_assert_true (ret);
            ret = gst_element_sync_state_with_parent(rtpopuspay);
            g_assert_true (ret);
        }

        //Change webrtcbin to send only
        g_signal_emit_by_name(webRtcPeerPtr->webrtcElement, "get-transceivers", &transceivers);
        if (transceivers) {
            GST_INFO("Changing webrtcbin for peer %s to sendonly. Number of transceivers(%d) ",
                     webRtcPeerPtr->peerId.c_str(),
                     transceivers->len);
            g_assert_true(transceivers->len > 0);
            for (int transRecvIndex = 0; transRecvIndex < transceivers->len; transRecvIndex++) {
                trans = g_array_index (transceivers, GstWebRTCRTPTransceiver *, transRecvIndex);
                trans->direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
            }
            g_array_unref(transceivers);
        }

        ret = gst_element_sync_state_with_parent(webRtcPeerPtr->webrtcElement);
        g_assert_true (ret);

        GST_INFO("Created webrtc bin for receiver peer %s", webRtcPeerPtr->peerId.c_str());
    }

    //Start timer to request sender keyframe every 1 sec
    if (video_valid) {
        g_timeout_add_seconds(1, send_key_frame_timer_callback, g_string_new(channelId.c_str()));
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

