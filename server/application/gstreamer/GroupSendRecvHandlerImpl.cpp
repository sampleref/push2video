//
// Created by chakra on 16-05-2020.
//
#define GST_USE_UNSTABLE_API

#include <thread>
#include <string>
#include <future> // std::async, std::future
#include <chrono>
#include <gst/webrtc/webrtc.h>
#include <gst/rtp/rtp.h>

#include "GstMediaUtils.hpp"
#include "../grpc/GrpcService.hpp"
#include "../utils/Push2TalkUtils.hpp"
#include "WebRtcPeer.hpp"
#include "GroupSendRecvHandler.hpp"

GST_DEBUG_CATEGORY_EXTERN (push2talk_gst);
#define GST_CAT_DEFAULT push2talk_gst

#define RTP_CAPS_OPUS "application/x-rtp,media=audio,encoding-name=OPUS,payload=111"
#define RTP_CAPS_VP8 "application/x-rtp,media=video,encoding-name=VP8,payload=96"

gboolean send_key_frame_timer_av_callback(gpointer user_data);

void launch_pipeline_sendrecv(std::string groupId);

void send_sdp_offer_av(GstWebRTCSessionDescription *offer, WebRtcPeer *webRtcPeer);

void release_group_pipeline(GroupSendRecvHandler *groupSendRecvHandler) {
    groupSendRecvHandler->close_pipeline_group();
}

gboolean pipeline_bus_callback_group(GstBus *bus, GstMessage *message, gpointer data) {
    switch (GST_MESSAGE_TYPE (message)) {
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug;
            gst_message_parse_error(message, &err, &debug);
            GroupSendRecvHandler *pipelineHandler = static_cast<GroupSendRecvHandler *>(data);
            GST_ERROR("pipeline_bus_callback:GST_MESSAGE_ERROR Error/code : %s/%d for group %s ", err->message,
                      err->code, pipelineHandler->groupId.c_str());
            release_group_pipeline(pipelineHandler);
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
            GroupSendRecvHandler *pipelineHandler = static_cast<GroupSendRecvHandler *>(data);
            GST_ERROR("pipeline_bus_callback:GST_MESSAGE_EOS for group %s ", pipelineHandler->groupId.c_str());
            release_group_pipeline(pipelineHandler);
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

bool GroupSendRecvHandler::create_group(std::string groupId) {
    GST_INFO("starting pipeline for group %s ", groupId.c_str());
    std::thread th(launch_pipeline_sendrecv, groupId);
    th.detach();
    return TRUE;
}

void launch_pipeline_sendrecv(std::string groupId) {

    GST_INFO("Creating pipeline groupId %s ", groupId.c_str());

    GroupSendRecvHandlerPtr pipelineHandlerPtr = push2talkUtils::fetch_groupsendrecvhandler_by_groupid(groupId);
    if (!pipelineHandlerPtr) {
        GST_INFO("ERROR - launch_new_pipeline: No matching pipeline handler found for group %s ",
                 groupId.c_str());
        return;
    }

    GstBus *bus;
    /* Create Elements */
    gchar *tmp = g_strdup_printf("group-pipeline-%s", groupId.c_str());
    pipelineHandlerPtr->pipeline = gst_pipeline_new(tmp);
    g_free(tmp);

    /*
     * Video Stream section
     */
    GstElement *videoinputselector, *videotee;
    tmp = g_strdup_printf("videoinputselector-%s", groupId.c_str());
    videoinputselector = gst_element_factory_make("input-selector", tmp);
    g_free(tmp);
    tmp = g_strdup_printf("videotee-%s", groupId.c_str());
    videotee = gst_element_factory_make("tee", tmp);
    g_free(tmp);

    if (!pipelineHandlerPtr->pipeline || !videotee) {
        GST_ERROR("launch_new_pipeline: Cannot create elements for video in group %s ", groupId.c_str());
        return;
    }

    /* Add Elements to pipeline and set properties */
    gst_bin_add_many(GST_BIN(pipelineHandlerPtr->pipeline), videoinputselector, videotee, NULL);
    g_object_set(videotee, "allow-not-linked", TRUE, NULL);

    if (!gst_element_link_many(videoinputselector, videotee, NULL)) {
        GST_ERROR("launch_new_pipeline: Error linking videoinputselector to videotee for group %s ",
                  groupId.c_str());
        return;
    }

    /*
     * Audio Stream section
     */
    GstElement *audioinputselector, *audiotee;
    tmp = g_strdup_printf("audioinputselector-%s", groupId.c_str());
    audioinputselector = gst_element_factory_make("input-selector", tmp);
    g_free(tmp);
    tmp = g_strdup_printf("audiotee-%s", groupId.c_str());
    audiotee = gst_element_factory_make("tee", tmp);
    g_free(tmp);

    if (!pipelineHandlerPtr->pipeline || !audiotee) {
        GST_ERROR("launch_new_pipeline: Cannot create elements for audio in group %s ", groupId.c_str());
        return;
    }

    //* Add Elements to pipeline and set properties *//*
    gst_bin_add_many(GST_BIN(pipelineHandlerPtr->pipeline), audioinputselector, audiotee, NULL);
    g_object_set(audiotee, "allow-not-linked", TRUE, NULL);

    if (!gst_element_link_many(audioinputselector, audiotee, NULL)) {
        GST_ERROR("launch_new_pipeline: Error linking audioinputselector to audiotee for group %s ",
                  groupId.c_str());
        return;
    }

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipelineHandlerPtr->pipeline));
    gst_bus_add_watch(bus, pipeline_bus_callback_group, pipelineHandlerPtr.get());
    gst_object_unref(GST_OBJECT(bus));

    GST_INFO("Starting pipeline for group %s ", groupId.c_str());
    GstStateChangeReturn return_val;
    return_val = gst_element_set_state(GST_ELEMENT (pipelineHandlerPtr->pipeline), GST_STATE_PLAYING);
    if (return_val == GST_STATE_CHANGE_FAILURE)
        goto err;

    g_timeout_add_seconds(1, send_key_frame_timer_av_callback, g_string_new(groupId.c_str()));

    pipelineHandlerPtr->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(pipelineHandlerPtr->loop);
    GST_INFO("Main Loop Stopping for pipeline group %s ", groupId.c_str());
    g_main_loop_unref(pipelineHandlerPtr->loop);
    GST_INFO("Main Loop Stopped for pipeline group %s ", groupId.c_str());
    return;

    err:
    GST_ERROR("launch_new_pipeline: State change failure for group pipeline %s ", groupId.c_str());
}

void GroupSendRecvHandler::close_pipeline_group() {
    if (pipeline) {
        try {
            for (auto elem : senderPeers) {
                WebRtcPeer webRtcPeerPtr = *(elem.second);
                reset_ue(webRtcPeerPtr.peerId, false, UeMediaDirection_Direction_SENDER);
            }
            GST_INFO("Clearing all sender peers list for group %s ", groupId.c_str());
            senderPeers.clear();
            for (auto elem : receiverPeers) {
                WebRtcPeer webRtcPeerPtr = *(elem.second);
                reset_ue(webRtcPeerPtr.peerId, false, UeMediaDirection_Direction_RECEIVER);
            }
            GST_INFO("Clearing all receiver peers list for group %s ", groupId.c_str());
            receiverPeers.clear();
        } catch (std::exception exception1) {
            GST_ERROR("stop_and_clear_allpeers_from_map ::  Exception Thrown %s ", exception1.what());
        }

        std::future<bool> futureStateNull = std::async(std::launch::async, [](string group_id) {
            GroupSendRecvHandlerPtr pipelineHandlerPtr = push2talkUtils::fetch_groupsendrecvhandler_by_groupid(
                    group_id);
            if (!pipelineHandlerPtr) {
                GST_ERROR("futureStateNull Async: No matching handler found for group %s ",
                          group_id.c_str());
                return false;
            }
            GST_INFO("futureStateNull Async: Setting state for group %s as NULL ", group_id.c_str());
            gst_element_set_state(GST_ELEMENT(pipelineHandlerPtr->pipeline), GST_STATE_NULL);
            return true;
        }, groupId);
        if (std::future_status::ready == futureStateNull.wait_for(std::chrono::milliseconds(150))) {
            GST_INFO("Nullified pipeline for channel %s with return %d ", groupId.c_str(), futureStateNull.get());
        } else {
            GST_INFO("Nullifying pipeline timeout for channel %s ", groupId.c_str());
        }
        pipeline = NULL;
    }
    if (loop) {
        GST_INFO("Stopping main loop for group %s ", groupId.c_str());
        g_main_quit(loop);
    }
    push2talkUtils::remove_groupsendrecv_handler(groupId);
}

/* Offer created by our audio pipeline, to be sent to the peer */
void on_offer_created_av(GstPromise *promise, gpointer user_data) {
    GstWebRTCSessionDescription *offer = NULL;
    const GstStructure *reply;
    WebRtcPeer *webRtcPeer = static_cast<WebRtcPeer *>(user_data);

    g_assert_cmphex (gst_promise_wait(promise), ==, GST_PROMISE_RESULT_REPLIED);
    reply = gst_promise_get_reply(promise);
    gst_structure_get(reply, "offer",
                      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);
    gst_promise_unref(promise);
    promise = gst_promise_new();
    g_signal_emit_by_name(webRtcPeer->webrtcElement, "set-local-description", offer, promise);
    gst_promise_interrupt(promise);
    gst_promise_unref(promise);

    /* Send offer to peer */
    GroupSendRecvHandlerPtr groupSendRecvHandlerPtr = push2talkUtils::fetch_groupsendrecvhandler_by_groupid(
            webRtcPeer->channelId);
    send_sdp_offer_av(offer, webRtcPeer);
    gst_webrtc_session_description_free(offer);
}

void on_negotiation_needed_av(GstElement *element, WebRtcPeer *user_data) {
    GstPromise *promise;
    promise = gst_promise_new_with_change_func(on_offer_created_av, user_data, NULL);
    g_signal_emit_by_name(user_data->webrtcElement, "create-offer", NULL, promise);
}

void send_ice_candidate_message_sendrecv(GstElement *webrtc G_GNUC_UNUSED, guint mlineindex,
                                         gchar *candidate, WebRtcPeer *user_data G_GNUC_UNUSED) {
    GST_DEBUG("send_ice_candidate_message of ue-group/index %s-%s / %d ", user_data->peerId.c_str(),
              user_data->channelId.c_str(),
              mlineindex);
    GroupSendRecvHandlerPtr groupSendRecvHandlerPtr = push2talkUtils::fetch_groupsendrecvhandler_by_groupid(
            user_data->channelId);
    groupSendRecvHandlerPtr->send_webrtc_ice_message(user_data->peerId, candidate, to_string(mlineindex),
                                                     user_data->direction);
}

void on_incoming_stream_sendrecv(GstElement *webrtc, GstPad *pad, WebRtcPeer *webRtcVideoPeer) {
    GST_INFO("Triggered on_incoming_stream ");
    gchar *dynamic_pad_name;
    dynamic_pad_name = gst_pad_get_name (pad);
    if (GST_PAD_DIRECTION (pad) != GST_PAD_SRC) {
        GST_WARNING("Incorrect pad direction %s ", dynamic_pad_name);
        return;
    }

    GstCaps *caps;
    const gchar *name;
    char *capsName;
    if (!gst_pad_has_current_caps(pad)) {
        GST_ERROR("Pad '%s' has no caps, can't do anything, ignoring",
                  GST_PAD_NAME(pad));
        return;
    }

    caps = gst_pad_get_current_caps(pad);
    capsName = gst_caps_to_string(caps);
    name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
    GST_INFO("on_incoming_stream caps name %s capsName %s ", name, capsName);
    GroupSendRecvHandlerPtr groupSendRecvHandlerPtr = push2talkUtils::fetch_groupsendrecvhandler_by_groupid(
            webRtcVideoPeer->channelId);

    if (g_strrstr(capsName, "audio")) {
        gchar *tmp;
        GstElement *rtpopusdepay;
        tmp = g_strdup_printf("rtpopusdepay-%s", webRtcVideoPeer->peerId.c_str());
        rtpopusdepay = gst_bin_get_by_name(GST_BIN (groupSendRecvHandlerPtr->pipeline), tmp);
        g_free(tmp);
        if (gst_element_link_pads(webrtc, dynamic_pad_name, rtpopusdepay, "sink")) {
            GST_INFO("webrtc pad %s linked to rtpopusdepay ", dynamic_pad_name);
        }
        gst_object_unref(rtpopusdepay);
    } else if (g_strrstr(capsName, "video")) {
        gchar *tmp;
        GstElement *rtpvp8depay;
        webRtcVideoPeer->videoSrcPadName = dynamic_pad_name;
        tmp = g_strdup_printf("rtpvp8depay-%s", webRtcVideoPeer->peerId.c_str());
        rtpvp8depay = gst_bin_get_by_name(GST_BIN (groupSendRecvHandlerPtr->pipeline), tmp);
        g_free(tmp);
        if (gst_element_link_pads(webrtc, dynamic_pad_name, rtpvp8depay, "sink")) {
            GST_INFO("webrtc pad %s linked to rtpvp8depay ", dynamic_pad_name);
        }
        gst_object_unref(rtpvp8depay);
    }
    g_free(dynamic_pad_name);
    g_free(capsName);
}

void on_new_ssrc_callback_av(GstElement *rtpbin, guint session, guint ssrc, gpointer udata) {
    WebRtcPeer *webRtcPeer = static_cast<WebRtcPeer *>(udata);
    GST_INFO("New SSRC created for session %d as %d for ue-direction %s-%s ", session, ssrc, webRtcPeer->peerId.c_str(),
             UeMediaDirection_Direction_Name(webRtcPeer->direction).c_str());
}

void send_sdp_offer_av(GstWebRTCSessionDescription *offer, WebRtcPeer *webRtcPeer) {
    gchar *text;

    text = gst_sdp_message_as_text(offer->sdp);
    GST_INFO("Sending offer for peer: %s \n %s", webRtcPeer->peerId.c_str(), text);

    push2talkUtils::fetch_groupsendrecvhandler_by_groupid(webRtcPeer->channelId)->send_sdp_offer(
            text, webRtcPeer->peerId);
    g_free(text);
}

void GroupSendRecvHandler::send_sdp_offer(std::string sdp, std::string ueId) {
    SdpOfferRequest sdpOfferRequest;
    sdpOfferRequest.set_ueid(ueId);
    sdpOfferRequest.set_groupid(groupId);
    sdpOfferRequest.set_sdp(sdp);
    sdpOfferRequest.mutable_uemediadirection()->set_direction(UeMediaDirection_Direction_RECEIVER);
    push2talkUtils::pushToTalkServiceClientPtr->sendSdpOfferRequest(sdpOfferRequest);
}

void GroupSendRecvHandler::apply_incoming_sdp(std::string ueId, std::string sdp, std::string type) {
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
                  ueId.c_str(),
                  sdp.c_str());
        reset_ue(ueId, true, UeMediaDirection_Direction_RECEIVER);
        return;
    }
    g_assert_cmpstr (sdptype, ==, "answer");

    GST_INFO("Received answer for receiver peer %s:\n%s", ueId.c_str(), sdp.c_str());

    ret = gst_sdp_message_new(&sdpMessage);
    g_assert_cmphex (ret, ==, GST_SDP_OK);

    ret = gst_sdp_message_parse_buffer((guint8 *) text, strlen(text), sdpMessage);
    if (ret != GST_SDP_OK) {
        GST_ERROR("Invalid SDP for receiver peer %s:\n%s", ueId.c_str(), sdp.c_str());
        reset_ue(ueId, true, UeMediaDirection_Direction_RECEIVER);
        return;
    }
    g_assert_cmphex (ret, ==, GST_SDP_OK);

    answer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER,
                                                sdpMessage);
    g_assert_nonnull (answer);

    /* Set remote description on our pipeline */
    {
        GstPromise *promise = gst_promise_new();
        g_signal_emit_by_name(fetch_peer_by_ueid(ueId, UeMediaDirection_Direction_RECEIVER)->webrtcElement,
                              "set-remote-description", answer,
                              promise);
        gst_promise_interrupt(promise);
        gst_promise_unref(promise);
    }
    GST_INFO("apply_incoming_sdp completed for receiver ue %s ", ueId.c_str());
}

void GroupSendRecvHandler::av_add_ue(std::string ueId, UeMediaDirection ueMediaDirection) {
    GST_INFO("Creating ue for group %s with ueId %s direction %s ", groupId.c_str(), ueId.c_str(),
             UeMediaDirection_Direction_Name(ueMediaDirection.direction()).c_str());
    WebRtcPeerPtr webRtcPeerPtrExisting = fetch_peer_by_ueid(ueId, ueMediaDirection.direction());
    if (webRtcPeerPtrExisting) {
        GST_ERROR("Already Ue Peer exists for ue %s direction %s Please exit and try again", ueId.c_str(),
                  UeMediaDirection_Direction_Name(ueMediaDirection.direction()).c_str());
        return;
    }
    int ret;
    gchar *tmp;
    GstPad *srcpad, *sinkpad;
    if (ueMediaDirection.direction() == UeMediaDirection_Direction_SENDER) {
        WebRtcPeerPtr webRtcPeerPtr = std::make_shared<WebRtcPeer>();
        webRtcPeerPtr->peerId = ueId;
        webRtcPeerPtr->channelId = groupId;
        //Create Elements:
        //Create webrtcbin
        webRtcPeerPtr->direction = UeMediaDirection_Direction_SENDER;
        tmp = g_strdup_printf("webrtcbin-send-%s", webRtcPeerPtr->peerId.c_str());
        webRtcPeerPtr->webrtcElement = gst_element_factory_make("webrtcbin", tmp);
        g_object_set(webRtcPeerPtr->webrtcElement, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_COMPAT, NULL);
        g_free(tmp);

        g_assert_nonnull (webRtcPeerPtr->webrtcElement);
        g_signal_connect (webRtcPeerPtr->webrtcElement, "on-ice-candidate",
                          G_CALLBACK(send_ice_candidate_message_sendrecv), webRtcPeerPtr.get());
        /* Incoming streams will be exposed via this signal */
        g_signal_connect (webRtcPeerPtr->webrtcElement, "pad-added", G_CALLBACK(on_incoming_stream_sendrecv),
                          webRtcPeerPtr.get());

        GstElement *rtpbin;
        rtpbin = gst_bin_get_by_name(GST_BIN(webRtcPeerPtr->webrtcElement), "rtpbin");
        if (rtpbin) {
            g_signal_connect (rtpbin, "on-new-ssrc", G_CALLBACK(on_new_ssrc_callback_av), webRtcPeerPtr.get());
            g_object_unref(rtpbin);
        }
        //Add elements to pipeline

        /*
        * Video Receiver Section
        */
        GstElement *rtpvp8depay, *videoinputselector, *videotee;
        tmp = g_strdup_printf("rtpvp8depay-%s", webRtcPeerPtr->peerId.c_str());
        rtpvp8depay = gst_element_factory_make("rtpvp8depay", tmp);
        g_free(tmp);

        //Add elements to pipeline
        gst_bin_add_many(GST_BIN (pipeline), rtpvp8depay, NULL);

        //Link rtpvp8depay -> videoinputselector
        tmp = g_strdup_printf("videoinputselector-%s", groupId.c_str());
        videoinputselector = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
        g_free(tmp);
        g_assert_nonnull (videoinputselector);
        srcpad = gst_element_get_static_pad(rtpvp8depay, "src");
        g_assert_nonnull (srcpad);
        sinkpad = gst_element_get_request_pad(videoinputselector, "sink_%u");
        g_assert_nonnull (sinkpad);
        webRtcPeerPtr->inputSelectorVideoPadName = gst_pad_get_name (sinkpad);
        //g_object_set(G_OBJECT (videoinputselector), "active-pad", sinkpad, NULL);
        ret = gst_pad_link(srcpad, sinkpad);
        g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
        gst_object_unref(srcpad);
        gst_object_unref(sinkpad);

        /*
        * Audio Receiver section
        */
        GstElement *rtpopusdepay, *opusdec, *audioinputselector, *audiotee;
        tmp = g_strdup_printf("rtpopusdepay-%s", webRtcPeerPtr->peerId.c_str());
        rtpopusdepay = gst_element_factory_make("rtpopusdepay", tmp);
        g_free(tmp);
        tmp = g_strdup_printf("opusdec-%s", webRtcPeerPtr->peerId.c_str());
        opusdec = gst_element_factory_make("opusdec", tmp);
        g_free(tmp);

        //Add elements to pipeline
        gst_bin_add_many(GST_BIN (pipeline), webRtcPeerPtr->webrtcElement, rtpopusdepay, opusdec, NULL);
        if (!gst_element_link_many(rtpopusdepay, opusdec, NULL)) {
            GST_ERROR("Error linking rtpopusdepay to opusdec/enc for ue/group %s/%s ", webRtcPeerPtr->peerId.c_str(),
                      groupId.c_str());
            return;
        }

        //Link opusdec -> audioinputselector
        tmp = g_strdup_printf("audioinputselector-%s", groupId.c_str());
        audioinputselector = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
        g_free(tmp);
        srcpad = gst_element_get_static_pad(opusdec, "src");
        g_assert_nonnull (srcpad);
        sinkpad = gst_element_get_request_pad(audioinputselector, "sink_%u");
        g_assert_nonnull (sinkpad);
        webRtcPeerPtr->inputSelectorAudioPadName = gst_pad_get_name (sinkpad);
        //g_object_set(G_OBJECT (audioinputselector), "active-pad", sinkpad, NULL);
        ret = gst_pad_link(srcpad, sinkpad);
        g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
        gst_object_unref(srcpad);
        gst_object_unref(sinkpad);

        ret = gst_element_sync_state_with_parent(webRtcPeerPtr->webrtcElement);
        g_assert_true (ret);
        ret = gst_element_sync_state_with_parent(rtpvp8depay);
        g_assert_true (ret);
        ret = gst_element_sync_state_with_parent(rtpopusdepay);
        g_assert_true (ret);
        ret = gst_element_sync_state_with_parent(opusdec);
        g_assert_true (ret);

        senderPeers[ueId] = webRtcPeerPtr; //Add to list of sender peers
        GST_INFO("Created webrtc bin for send ue %s for group %s", webRtcPeerPtr->peerId.c_str(), groupId.c_str());
    } else if (ueMediaDirection.direction() == UeMediaDirection_Direction_RECEIVER) {
        WebRtcPeerPtr webRtcPeerPtr = std::make_shared<WebRtcPeer>();
        webRtcPeerPtr->peerId = ueId;
        webRtcPeerPtr->channelId = groupId;
        //Create Elements:
        //Create webrtcbin
        webRtcPeerPtr->direction = UeMediaDirection_Direction_RECEIVER;
        tmp = g_strdup_printf("webrtcbin-recv-%s", webRtcPeerPtr->peerId.c_str());
        webRtcPeerPtr->webrtcElement = gst_element_factory_make("webrtcbin", tmp);
        g_object_set(webRtcPeerPtr->webrtcElement, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE, NULL);
        g_free(tmp);

        g_assert_nonnull (webRtcPeerPtr->webrtcElement);
        g_signal_connect (webRtcPeerPtr->webrtcElement, "on-negotiation-needed",
                          G_CALLBACK(on_negotiation_needed_av), webRtcPeerPtr.get());
        g_signal_connect (webRtcPeerPtr->webrtcElement, "on-ice-candidate",
                          G_CALLBACK(send_ice_candidate_message_sendrecv), webRtcPeerPtr.get());

        GstElement *rtpbin;
        rtpbin = gst_bin_get_by_name(GST_BIN(webRtcPeerPtr->webrtcElement), "rtpbin");
        if (rtpbin) {
            g_signal_connect (rtpbin, "on-new-ssrc", G_CALLBACK(on_new_ssrc_callback_av), webRtcPeerPtr.get());
            g_object_unref(rtpbin);
        }

        //Add elements to pipeline
        gst_bin_add_many(GST_BIN (pipeline), webRtcPeerPtr->webrtcElement, NULL);

        GstCaps *caps;
        /*
        * Video Sender section
        */
        GstElement *videotee, *videoqueue, *rtpvp8pay;

        //Create queue
        tmp = g_strdup_printf("queue_video-%s", webRtcPeerPtr->peerId.c_str());
        videoqueue = gst_element_factory_make("queue", tmp);
        g_object_set(videoqueue, "leaky", 2, NULL);
        g_free(tmp);

        //Create rtpvp8pay with caps
        tmp = g_strdup_printf("rtpvp8pay-%s", webRtcPeerPtr->peerId.c_str());
        rtpvp8pay = gst_element_factory_make("rtpvp8pay", tmp);
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
        tmp = g_strdup_printf("videotee-%s", groupId.c_str());
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

        /*
        * Audio Sender Section
        */
        GstElement *audiotee, *audioqueue, *opusenc, *rtpopuspay;

        //Create queue
        tmp = g_strdup_printf("queue_audio-%s", webRtcPeerPtr->peerId.c_str());
        audioqueue = gst_element_factory_make("queue", tmp);
        g_object_set(audioqueue, "leaky", 2, NULL);
        g_free(tmp);

        tmp = g_strdup_printf("opusenc-%s", webRtcPeerPtr->peerId.c_str());
        opusenc = gst_element_factory_make("opusenc", tmp);
        g_free(tmp);

        //Create rtpopuspay with caps
        tmp = g_strdup_printf("rtpopuspay-%s", webRtcPeerPtr->peerId.c_str());
        rtpopuspay = gst_element_factory_make("rtpopuspay", tmp);
        g_free(tmp);

        srcpad = gst_element_get_static_pad(rtpopuspay, "src");
        caps = gst_caps_from_string(RTP_CAPS_OPUS);
        gst_pad_set_caps(srcpad, caps);
        gst_caps_unref(caps);
        gst_object_unref(srcpad);

        //Add elements to pipeline
        gst_bin_add_many(GST_BIN (pipeline), audioqueue, opusenc, rtpopuspay, NULL);

        //Link queue -> opusenc
        srcpad = gst_element_get_static_pad(audioqueue, "src");
        g_assert_nonnull (srcpad);
        sinkpad = gst_element_get_static_pad(opusenc, "sink");
        g_assert_nonnull (sinkpad);
        ret = gst_pad_link(srcpad, sinkpad);
        g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
        gst_object_unref(srcpad);
        gst_object_unref(sinkpad);

        //Link opusenc -> rtpopuspay
        srcpad = gst_element_get_static_pad(opusenc, "src");
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
        tmp = g_strdup_printf("audiotee-%s", groupId.c_str());
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

        //Change webrtcbin to send only
        GstWebRTCRTPTransceiver *trans;
        GArray *transceivers;
        g_signal_emit_by_name(webRtcPeerPtr->webrtcElement, "get-transceivers", &transceivers);
        if (transceivers) {
            GST_INFO("Update webrtcbin for ue %s to sendonly. Number of transceivers(%d) ",
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
        ret = gst_element_sync_state_with_parent(videoqueue);
        g_assert_true (ret);
        ret = gst_element_sync_state_with_parent(rtpvp8pay);
        g_assert_true (ret);
        ret = gst_element_sync_state_with_parent(audioqueue);
        g_assert_true (ret);
        ret = gst_element_sync_state_with_parent(opusenc);
        g_assert_true (ret);
        ret = gst_element_sync_state_with_parent(rtpopuspay);
        g_assert_true (ret);

        receiverPeers[ueId] = webRtcPeerPtr; //Add to list of receiver peers
        GST_INFO("Created webrtc bin for recv ue %s for group %s", webRtcPeerPtr->peerId.c_str(), groupId.c_str());
    } else {
        GST_ERROR("Invalid Direction");
    }
}

void GroupSendRecvHandler::ue_ice_message(std::string icecandidate, std::string mlineindex, std::string ueId,
                                          UeMediaDirection ueMediaDirection) {
    GST_DEBUG("Applying ice candidate %s mLineIndex %s for ue %s direction %s ", icecandidate.c_str(),
              mlineindex.c_str(),
              ueId.c_str(),
              UeMediaDirection_Direction_Name(ueMediaDirection.direction()).c_str());
    int mLineindex = std::stoi(mlineindex);
    const gchar *candidateMsg;
    candidateMsg = icecandidate.c_str();
    WebRtcPeerPtr webRtcPeerPtr = fetch_peer_by_ueid(ueId, ueMediaDirection.direction());
    if (webRtcPeerPtr != NULL) {
        while ((webRtcPeerPtr->webrtcElement == NULL) || !(G_TYPE_CHECK_INSTANCE(webRtcPeerPtr->webrtcElement))) {
            GST_INFO("Waiting for ICE to be applied for peer %s ", ueId.c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        g_signal_emit_by_name(webRtcPeerPtr->webrtcElement, "add-ice-candidate", mLineindex, candidateMsg);
    } else {
        GST_ERROR("No WebRTCPeerPtr found for ue %s direction %s ", ueId.c_str(),
                  UeMediaDirection_Direction_Name(ueMediaDirection.direction()).c_str());
    }
}

UeFloorControlResponse_Action GroupSendRecvHandler::update_ue_floor_control(std::string ueId,
                                                                            UeFloorControlRequest_Action floorControlRequestAction) {
    GST_INFO("Floor control action %s from ue %s for group %s ",
             UeFloorControlRequest_Action_Name(floorControlRequestAction).c_str(), ueId.c_str(), groupId.c_str());
    switch (floorControlRequestAction) {
        case UeFloorControlRequest_Action_ACQUIRE:
            if (currentSenderUeId.empty()) {
                allocate_floor_control_to_ue(ueId);
                return UeFloorControlResponse_Action_GRANTED;
            } else {
                return UeFloorControlResponse_Action_REJECTED;
            }
        case UeFloorControlRequest_Action_RELEASE:
            if (!currentSenderUeId.empty() && currentSenderUeId.compare(ueId) == 0) {
                revoke_floor_control_from_ue(ueId);
                return UeFloorControlResponse_Action_DONE;
            } else {
                return UeFloorControlResponse_Action_INVALID_UE;
            }
        default:
            GST_ERROR("Invalid control request on group %s from ue %s", groupId.c_str(), ueId.c_str());
    }
}

void
GroupSendRecvHandler::reset_ue(std::string ueId, bool removeFromPeers, UeMediaDirection_Direction ueMediaDirection) {
    gchar *tmp;
    GstPad *srcpad, *sinkpad;
    GstElement *webrtc, *rtpvp8depay, *rtpopusdepay, *videoinputselector, *audioinputselector,
            *rtpvp8pay, *rtpopuspay, *videoqueue, *audioqueue, *videotee, *audiotee, *opusenc, *opusdec;

    GST_INFO("Remove ue %s from group %s direction %s ", ueId.c_str(), groupId.c_str(),
             UeMediaDirection_Direction_Name(ueMediaDirection).c_str());

    if (ueMediaDirection == UeMediaDirection_Direction_SENDER) {
        tmp = g_strdup_printf("webrtcbin-send-%s", ueId.c_str());
    } else if (ueMediaDirection == UeMediaDirection_Direction_RECEIVER) {
        tmp = g_strdup_printf("webrtcbin-recv-%s", ueId.c_str());
    }
    webrtc = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    g_free(tmp);

    if (webrtc) {
        GST_INFO("Removing existing webrtcbin for remote ue %s ", ueId.c_str());
        gst_element_set_state(webrtc, GST_STATE_NULL);
        gst_bin_remove(GST_BIN (pipeline), webrtc);
        gst_object_unref(webrtc);
    } else {
        GST_ERROR("No existing webrtcbin found for ue %s ", ueId.c_str());
        remove_peer_by_ueid(ueId, ueMediaDirection);
        return;
    }

    if (ueMediaDirection == UeMediaDirection_Direction_SENDER) {
        tmp = g_strdup_printf("opusdec-%s", ueId.c_str());
        opusdec = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
        g_free(tmp);
        if (opusdec) {
            gst_element_set_state(opusdec, GST_STATE_NULL);
            srcpad = gst_element_get_static_pad(opusdec, "src");
            g_assert_nonnull (srcpad);
            sinkpad = gst_pad_get_peer(srcpad);
            g_assert_nonnull (sinkpad);
            gst_object_unref(srcpad);

            gst_bin_remove(GST_BIN (pipeline), opusdec);
            gst_object_unref(opusdec);

            tmp = g_strdup_printf("audioinputselector-%s", groupId.c_str());
            audioinputselector = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
            g_free(tmp);
            if (audioinputselector) {
                g_assert_nonnull (audioinputselector);
                gst_element_release_request_pad(audioinputselector, sinkpad);
                gst_object_unref(sinkpad);
                gst_object_unref(audioinputselector);
            }
        }

        tmp = g_strdup_printf("rtpvp8depay-%s", ueId.c_str());
        rtpvp8depay = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
        g_free(tmp);
        if (rtpvp8depay) {
            gst_element_set_state(rtpvp8depay, GST_STATE_NULL);
            srcpad = gst_element_get_static_pad(rtpvp8depay, "src");
            g_assert_nonnull (srcpad);
            sinkpad = gst_pad_get_peer(srcpad);
            g_assert_nonnull (sinkpad);
            gst_object_unref(srcpad);

            gst_bin_remove(GST_BIN (pipeline), rtpvp8depay);
            gst_object_unref(rtpvp8depay);

            tmp = g_strdup_printf("videoinputselector-%s", groupId.c_str());
            videoinputselector = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
            g_free(tmp);
            if (videoinputselector) {
                g_assert_nonnull (videoinputselector);
                gst_element_release_request_pad(videoinputselector, sinkpad);
                gst_object_unref(sinkpad);
                gst_object_unref(videoinputselector);
            }
        }

        tmp = g_strdup_printf("rtpopusdepay-%s", ueId.c_str());
        rtpopusdepay = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
        g_free(tmp);
        if (rtpopusdepay) {
            gst_element_set_state(rtpopusdepay, GST_STATE_NULL);
            gst_bin_remove(GST_BIN(pipeline), rtpopusdepay);
            gst_object_unref(rtpopusdepay);
        }

        GST_INFO("Removed webrtcbin send for remote ue : %s", ueId.c_str());
        if (removeFromPeers) {
            remove_peer_by_ueid(ueId, ueMediaDirection);
        }
    } else if (ueMediaDirection == UeMediaDirection_Direction_RECEIVER) {
        tmp = g_strdup_printf("rtpvp8pay-%s", ueId.c_str());
        rtpvp8pay = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
        g_free(tmp);
        if (rtpvp8pay) {
            gst_element_set_state(rtpvp8pay, GST_STATE_NULL);
            gst_bin_remove(GST_BIN(pipeline), rtpvp8pay);
            gst_object_unref(rtpvp8pay);
        }

        tmp = g_strdup_printf("rtpopuspay-%s", ueId.c_str());
        rtpopuspay = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
        g_free(tmp);
        if (rtpopuspay) {
            gst_element_set_state(rtpopuspay, GST_STATE_NULL);
            gst_bin_remove(GST_BIN(pipeline), rtpopuspay);
            gst_object_unref(rtpopuspay);
        }

        tmp = g_strdup_printf("queue_video-%s", ueId.c_str());
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

            tmp = g_strdup_printf("videotee-%s", ueId.c_str());
            videotee = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
            g_free(tmp);
            if (videotee) {
                g_assert_nonnull (videotee);
                gst_element_release_request_pad(videotee, srcpad);
                gst_object_unref(srcpad);
                gst_object_unref(videotee);
            }
        }

        tmp = g_strdup_printf("opusenc-%s", ueId.c_str());
        opusenc = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
        g_free(tmp);
        if (opusenc) {
            gst_element_set_state(opusenc, GST_STATE_NULL);
            gst_bin_remove(GST_BIN (pipeline), opusenc);
            gst_object_unref(opusenc);
        }

        tmp = g_strdup_printf("queue_audio-%s", ueId.c_str());
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

            tmp = g_strdup_printf("audiotee-%s", ueId.c_str());
            audiotee = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
            g_free(tmp);
            if (audiotee) {
                g_assert_nonnull (audiotee);
                gst_element_release_request_pad(audiotee, srcpad);
                gst_object_unref(srcpad);
                gst_object_unref(audiotee);
            }
        }

        GST_INFO("Removed webrtcbin recv for remote ue : %s", ueId.c_str());
        if (removeFromPeers) {
            remove_peer_by_ueid(ueId, ueMediaDirection);
        }
    }
}

gboolean send_key_frame_timer_av_callback(gpointer user_data) {
    GString *val = (GString *) user_data;
    GroupSendRecvHandlerPtr groupSendRecvHandlerPtr = push2talkUtils::fetch_groupsendrecvhandler_by_groupid(val->str);
    if (groupSendRecvHandlerPtr) {
        groupSendRecvHandlerPtr->send_key_frame_request_to_sender();
        return TRUE;
    }
    GST_INFO("AV pipeline handler invalid for group %s. Might be stopped ", val->str);
    g_string_free(val, TRUE);
    return FALSE;
}

void GroupSendRecvHandler::send_key_frame_request_to_sender() {
    GST_DEBUG("Sending key frame request for sender peer %s ", currentSenderUeId.c_str());
    if (pipeline) {
        if (currentSenderUeId.empty()) {
            GST_DEBUG("Current sender is none, not sending any keyframe request");
            return;
        }
        GstPad *videosrcpad;
        WebRtcPeerPtr webRtcPeerPtr = fetch_peer_by_ueid(currentSenderUeId, UeMediaDirection_Direction_SENDER);
        if (!webRtcPeerPtr) {
            return;
        }
        videosrcpad = gst_element_get_static_pad(webRtcPeerPtr->webrtcElement, webRtcPeerPtr->videoSrcPadName.c_str());
        if (videosrcpad) {
            gst_pad_send_event(videosrcpad, gst_event_new_custom(GST_EVENT_CUSTOM_UPSTREAM,
                                                                 gst_structure_new("GstForceKeyUnit", "all-headers",
                                                                                   G_TYPE_BOOLEAN, TRUE, NULL)));
        } else {
            GST_ERROR("Sender webrtcbin video src pad %s not exists for sender peer %s ",
                      webRtcPeerPtr->videoSrcPadName.c_str(), currentSenderUeId.c_str());
        }
    } else {
        GST_ERROR("Pipeline not exists for sender peer %s ", currentSenderUeId.c_str());
    }
}

void GroupSendRecvHandler::allocate_floor_control_to_ue(std::string ueId) {
    GST_INFO("Allocating floor control to ue %s for group %s ", ueId.c_str(), groupId.c_str());
    gchar *tmp;
    GstElement *videoInputSelector, *audioInputSelector;
    tmp = g_strdup_printf("audioinputselector-%s", groupId.c_str());
    audioInputSelector = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    g_free(tmp);
    tmp = g_strdup_printf("videoinputselector-%s", groupId.c_str());
    videoInputSelector = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    g_free(tmp);
    GstPad *audioPad, *videoPad;
    WebRtcPeerPtr webRtcPeerPtr = fetch_peer_by_ueid(ueId, UeMediaDirection_Direction_SENDER);
    audioPad = gst_element_get_static_pad(audioInputSelector, webRtcPeerPtr->inputSelectorAudioPadName.c_str());
    videoPad = gst_element_get_static_pad(videoInputSelector, webRtcPeerPtr->inputSelectorVideoPadName.c_str());
    g_object_set(G_OBJECT (audioInputSelector), "active-pad", audioPad, NULL);
    g_object_set(G_OBJECT (videoInputSelector), "active-pad", videoPad, NULL);
    gst_object_unref(audioPad);
    gst_object_unref(videoPad);
    currentSenderUeId = ueId;
}

void GroupSendRecvHandler::revoke_floor_control_from_ue(std::string ueId) {
    GST_INFO("Removing floor control from ue %s for group %s ", ueId.c_str(), groupId.c_str());
    gchar *tmp;
    GstElement *videoInputSelector, *audioInputSelector;
    tmp = g_strdup_printf("audioinputselector-%s", groupId.c_str());
    audioInputSelector = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    g_free(tmp);
    tmp = g_strdup_printf("videoinputselector-%s", groupId.c_str());
    videoInputSelector = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    g_free(tmp);
    g_object_set(G_OBJECT (audioInputSelector), "active-pad", NULL, NULL);
    g_object_set(G_OBJECT (videoInputSelector), "active-pad", NULL, NULL);
    currentSenderUeId = "";
}

WebRtcPeerPtr GroupSendRecvHandler::fetch_peer_by_ueid(std::string ueId, UeMediaDirection_Direction direction) {
    std::lock_guard<std::mutex> lockGuard(push2talkUtils::peers_mutex);
    GST_DEBUG("Fetching peer with ueId %s ", ueId.c_str());
    if (direction == UeMediaDirection_Direction_SENDER) {
        auto it_peer = senderPeers.find(ueId);
        if (it_peer != senderPeers.end()) {
            return it_peer->second;
        } else {
            return NULL;
        }
    } else if (direction == UeMediaDirection_Direction_RECEIVER) {
        auto it_peer = receiverPeers.find(ueId);
        if (it_peer != receiverPeers.end()) {
            return it_peer->second;
        } else {
            return NULL;
        }
    }
}

void GroupSendRecvHandler::remove_peer_by_ueid(std::string ueId,
                                               UeMediaDirection_Direction direction) {
    std::lock_guard<std::mutex> lockGuard(push2talkUtils::peers_mutex);
    if (direction == UeMediaDirection_Direction_SENDER) {
        auto it_peer = senderPeers.find(ueId);
        if (it_peer != senderPeers.end()) {
            senderPeers.erase(it_peer);
            GST_INFO("Deleted sender peer from map for ue %s", ueId.c_str());
        }
    } else if (direction == UeMediaDirection_Direction_RECEIVER) {
        auto it_peer = receiverPeers.find(ueId);
        if (it_peer != receiverPeers.end()) {
            receiverPeers.erase(it_peer);
            GST_INFO("Deleted receiver peer from map for ue %s", ueId.c_str());
        }
    }
}

void GroupSendRecvHandler::send_webrtc_answer_sdp(std::string ueId, std::string sdp,
                                                  UeMediaDirection_Direction direction) {
    SdpAnswerRequest sdpAnswerRequest;
    sdpAnswerRequest.set_ueid(ueId);
    sdpAnswerRequest.set_groupid(groupId);
    sdpAnswerRequest.set_sdp(sdp);
    sdpAnswerRequest.mutable_uemediadirection()->set_direction(direction);
    push2talkUtils::pushToTalkServiceClientPtr->sendSdpAnswerRequest(sdpAnswerRequest);
}

void GroupSendRecvHandler::send_webrtc_ice_message(std::string ueId, std::string ice, std::string mLineIndex,
                                                   UeMediaDirection_Direction direction) {
    IceMessageRequest iceMessageRequest;
    iceMessageRequest.set_groupid(groupId);
    iceMessageRequest.set_ueid(ueId);
    iceMessageRequest.set_ice(ice);
    iceMessageRequest.set_mlineindex(mLineIndex);
    iceMessageRequest.mutable_uemediadirection()->set_direction(direction);
    push2talkUtils::pushToTalkServiceClientPtr->sendIceMessageRequest(iceMessageRequest);
}

/* Answer created by our pipeline, to be sent to the ue */
static void on_answer_created_sendrecv(GstPromise *promise, WebRtcPeer *webRtcVideoPeer) {
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
    push2talkUtils::fetch_groupsendrecvhandler_by_groupid(webRtcVideoPeer->channelId)->send_webrtc_answer_sdp(
            webRtcVideoPeer->peerId, text, webRtcVideoPeer->direction);
    gst_webrtc_session_description_free(answer);
}

void GroupSendRecvHandler::apply_webrtc_peer_sdp(std::string ueId, std::string sdp,
                                                 std::string type, UeMediaDirection ueMediaDirection) {
    GST_INFO("Received SDP type %s for ue %s in group %s ", type.c_str(), ueId.c_str(), groupId.c_str());
    WebRtcPeerPtr webRtcPeerPtr = fetch_peer_by_ueid(ueId, ueMediaDirection.direction());
    if (webRtcPeerPtr == NULL) {
        GST_ERROR("No valid webrtc peer exists");
        return;
    }
    g_assert_cmpstr (type.c_str(), ==, "offer");
    int ret;
    GstSDPMessage *sdpMsg;
    const gchar *text;
    GstWebRTCSessionDescription *offer;
    text = sdp.c_str();
    GST_INFO("Received offer:\n %s", text);

    ret = gst_sdp_message_new(&sdpMsg);
    g_assert_cmphex (ret, ==, GST_SDP_OK);

    ret = gst_sdp_message_parse_buffer((guint8 *) text, strlen(text), sdpMsg);
    g_assert_cmphex (ret, ==, GST_SDP_OK);

    offer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_OFFER, sdpMsg);
    g_assert_nonnull (offer);

    /* Set remote description on our pipeline */
    {
        GstPromise *promise = gst_promise_new();
        g_signal_emit_by_name(webRtcPeerPtr->webrtcElement, "set-remote-description", offer,
                              promise);
        gst_promise_interrupt(promise);
        gst_promise_unref(promise);

        /* Create an answer that we will send back to the ue */
        promise = gst_promise_new_with_change_func((GstPromiseChangeFunc) on_answer_created_sendrecv,
                                                   (gpointer) webRtcPeerPtr.get(), NULL);
        g_signal_emit_by_name(webRtcPeerPtr->webrtcElement, "create-answer", NULL, promise);
    }
}