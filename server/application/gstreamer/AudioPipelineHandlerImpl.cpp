//
// Created by chakra on 03-03-2020.
//
#define GST_USE_UNSTABLE_API

#include <thread>
#include <string>
#include <gst/webrtc/webrtc.h>

#include "AudioPipelineHandler.hpp"
#include "../grpc/GrpcService.hpp"
#include "../utils/Push2TalkUtils.hpp"
#include "WebRtcPeer.hpp"

GST_DEBUG_CATEGORY_EXTERN (push2talk_gst);
#define GST_CAT_DEFAULT push2talk_gst

gboolean pipeline_bus_callback_audio(GstBus *bus, GstMessage *message, gpointer data) {
    switch (GST_MESSAGE_TYPE (message)) {
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug;
            gst_message_parse_error(message, &err, &debug);
            AudioPipelineHandler *pipelineHandler = static_cast<AudioPipelineHandler *>(data);
            GST_ERROR("pipeline_bus_callback:GST_MESSAGE_ERROR Error/code : %s/%d for meeting %s ", err->message,
                      err->code, pipelineHandler->channelId.c_str());
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
            AudioPipelineHandler *pipelineHandler = static_cast<AudioPipelineHandler *>(data);
            GST_ERROR("pipeline_bus_callback:GST_MESSAGE_EOS for meeting %s ", pipelineHandler->channelId.c_str());
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

WebRtcPeerPtr AudioPipelineHandler::fetch_audio_sender_by_peerid(std::string peerId) {
    std::lock_guard<std::mutex> lockGuard(push2talkUtils::peers_mutex);
    GST_INFO("Fetching sender peer with peerid %s ", peerId.c_str());
    auto it_peer = peerAudioSenders.find(peerId);
    if (it_peer != peerAudioSenders.end()) {
        return it_peer->second;
    } else {
        return NULL;
    }
}

WebRtcPeerPtr AudioPipelineHandler::fetch_audio_reciever_by_peerid(std::string peerId) {
    std::lock_guard<std::mutex> lockGuard(push2talkUtils::peers_mutex);
    GST_INFO("Fetching receiver peer with peerid %s ", peerId.c_str());
    auto it_peer = peerAudioReceivers.find(peerId);
    if (it_peer != peerAudioReceivers.end()) {
        return it_peer->second;
    } else {
        return NULL;
    }
}

void AudioPipelineHandler::remove_audio_sender_by_peerid(std::string peerId) {
    std::lock_guard<std::mutex> lockGuard(push2talkUtils::peers_mutex);
    auto it_peer = peerAudioSenders.find(peerId);
    if (it_peer != peerAudioSenders.end()) {
        peerAudioSenders.erase(it_peer);
        GST_INFO("Deleted Sender peer from map for peer %s", peerId.c_str());
    }
}

void AudioPipelineHandler::remove_audio_reciever_by_peerid(std::string peerId) {
    std::lock_guard<std::mutex> lockGuard(push2talkUtils::peers_mutex);
    auto it_peer = peerAudioReceivers.find(peerId);
    if (it_peer != peerAudioReceivers.end()) {
        peerAudioSenders.erase(it_peer);
        GST_INFO("Deleted Receiver peer from map for peer %s", peerId.c_str());
    }
}


void launch_pipeline_audio(std::string meetingId);

gboolean AudioPipelineHandler::start_pipeline() {
    GST_INFO("starting pipeline for meeting %s ", channelId.c_str());
    std::thread th(launch_pipeline_audio, channelId);
    th.detach();
    return TRUE;
}

void launch_pipeline_audio(std::string meetingId) {
    GST_INFO("Creating common audio pipeline meetingId %s ", meetingId.c_str());

    AudioPipelineHandlerPtr pipelineHandlerPtr = push2talkUtils::fetch_audio_pipelinehandler_by_key(meetingId);
    if (!pipelineHandlerPtr) {
        GST_INFO("ERROR - launch_new_pipeline: No matching audio pipeline handler found for meeting %s ",
                 meetingId.c_str());
        return;
    }

    int ret;
    GstBus *bus;
    GstElement *audiotestsrc, *capsfilter, *audiomixer, *audioconvert, *audiotee;
    GstCaps *caps;
    GstPad *srcpad, *sinkpad;

    /* Create Elements */
    gchar *tmp = g_strdup_printf("audio-pipeline");
    pipelineHandlerPtr->pipeline = gst_pipeline_new(tmp);
    g_free(tmp);
    audiotestsrc = gst_element_factory_make("audiotestsrc", "audiotestsrc0");
    audiomixer = gst_element_factory_make("audiomixer", "audiomixer0");
    audioconvert = gst_element_factory_make("audioconvert", "audioconvert0");
    audiotee = gst_element_factory_make("tee", "audiotee0");

    capsfilter = gst_element_factory_make("capsfilter", "audiotestsrc0-capsfilter");
    caps = gst_caps_from_string(
            "audio/x-raw, rate=(int)48000, format=(string)S16LE, channels=(int)1, layout=(string)interleaved");
    g_object_set(capsfilter, "caps", caps, NULL);
    gst_caps_unref(caps);

    if (!pipelineHandlerPtr->pipeline || !audiotestsrc || !capsfilter || !audiomixer || !audioconvert || !audiotee) {
        GST_ERROR("launch_new_pipeline: Cannot create elements for %s ", "base pipeline");
        return;
    }

    /* Add Elements to pipeline and set properties */
    //gst_bin_add_many(GST_BIN(pipelineHandlerPtr->pipeline), audiotestsrc, capsfilter, audiomixer, audioconvert, audiotee,
    //               NULL);
    gst_bin_add_many(GST_BIN(pipelineHandlerPtr->pipeline), audiomixer, audioconvert, audiotee, NULL);
    g_object_set(audiotestsrc, "num-buffers", -1, NULL);
    g_object_set(audiotestsrc, "freq", 0, NULL);
    g_object_set(audiotestsrc, "is-live", TRUE, NULL);
    g_object_set(audiotee, "allow-not-linked", TRUE, NULL);

    //Link audiotestsrc -> capsfilter
    /*
    srcpad = gst_element_get_static_pad(audiotestsrc, "src");
    g_assert_nonnull (srcpad);
    sinkpad = gst_element_get_static_pad(capsfilter, "sink");
    g_assert_nonnull (sinkpad);
    ret = gst_pad_link(srcpad, sinkpad);
    g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
    gst_object_unref(srcpad);
    gst_object_unref(sinkpad);

    //Link capsfilter -> audiomixer
    srcpad = gst_element_get_static_pad(capsfilter, "src");
    g_assert_nonnull (srcpad);
    sinkpad = gst_element_get_request_pad(audiomixer, "sink_%u");
    g_assert_nonnull (sinkpad);
    ret = gst_pad_link(srcpad, sinkpad);
    g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
    gst_object_unref(srcpad);
    gst_object_unref(sinkpad);
    GST_DEBUG("Linked audiotestsrc to audiomixer ");
    */

    //Link audiomixer -> audioconvert
    srcpad = gst_element_get_static_pad(audiomixer, "src");
    g_assert_nonnull (srcpad);
    sinkpad = gst_element_get_static_pad(audioconvert, "sink");
    g_assert_nonnull (sinkpad);
    ret = gst_pad_link(srcpad, sinkpad);
    g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
    gst_object_unref(srcpad);
    gst_object_unref(sinkpad);
    GST_DEBUG("Linked audiomixer to audioconvert ");

    //Link audioconvert -> audiotee
    srcpad = gst_element_get_static_pad(audioconvert, "src");
    g_assert_nonnull (srcpad);
    sinkpad = gst_element_get_static_pad(audiotee, "sink");
    g_assert_nonnull (sinkpad);
    ret = gst_pad_link(srcpad, sinkpad);
    g_assert_cmpint (ret, ==, GST_PAD_LINK_OK);
    gst_object_unref(srcpad);
    gst_object_unref(sinkpad);
    GST_DEBUG("Linked audioconvert to audiotee ");

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipelineHandlerPtr->pipeline));
    gst_bus_add_watch(bus, pipeline_bus_callback_audio, pipelineHandlerPtr.get());
    gst_object_unref(GST_OBJECT(bus));

    GST_INFO("launch_new_pipeline: Starting pipeline, not transmitting yet");
    GstStateChangeReturn return_val;
    return_val = gst_element_set_state(GST_ELEMENT (pipelineHandlerPtr->pipeline), GST_STATE_PLAYING);
    if (return_val == GST_STATE_CHANGE_FAILURE)
        goto err;

    pipelineHandlerPtr->loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(pipelineHandlerPtr->loop);
    GST_INFO("Main Loop Stopping for base audio pipeline");
    g_main_loop_unref(pipelineHandlerPtr->loop);
    GST_INFO("Main Loop Stopped for base audio pipeline");
    return;

    err:
    GST_ERROR("launch_new_pipeline: State change failure for audio pipeline");
}

void send_ice_candidate_message_audio(GstElement *webrtc G_GNUC_UNUSED, guint mlineindex,
                                      gchar *candidate, WebRtcPeer *user_data G_GNUC_UNUSED) {
    GST_INFO("send_ice_candidate_message of peer/direction/index %s / %d / %d ", candidate,
             user_data->peerDirection, mlineindex);
    AudioPipelineHandlerPtr audioPipelineHandlerPtr = push2talkUtils::fetch_audio_pipelinehandler_by_key(
            user_data->channelId);
    if (SENDER == user_data->peerDirection) {
        audioPipelineHandlerPtr->send_webrtc_audio_sender_ice(user_data->peerId, candidate, to_string(mlineindex));
    } else if (RECEIVER == user_data->peerDirection) {
        audioPipelineHandlerPtr->send_webrtc_audio_receiver_ice(user_data->peerId, candidate, to_string(mlineindex));
    } else {
        GST_ERROR("Unknown sender/receiver type!");
    }
}

/* Answer created by our pipeline, to be sent to the peer */
void on_answer_created_audio(GstPromise *promise, WebRtcPeer *webRtcAudioPeer) {
    GstWebRTCSessionDescription *answer;
    const GstStructure *reply;

    g_assert_cmpint (gst_promise_wait(promise), ==, GST_PROMISE_RESULT_REPLIED);
    reply = gst_promise_get_reply(promise);
    gst_structure_get(reply, "answer",
                      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);
    gst_promise_unref(promise);

    promise = gst_promise_new();
    g_assert_nonnull (webRtcAudioPeer->webrtcElement);
    g_signal_emit_by_name(webRtcAudioPeer->webrtcElement, "set-local-description", answer, promise);
    gst_promise_interrupt(promise);
    gst_promise_unref(promise);

    /* Send answer to peer */
    gchar *text;
    text = gst_sdp_message_as_text(answer->sdp);
    push2talkUtils::fetch_audio_pipelinehandler_by_key(webRtcAudioPeer->channelId)->send_webrtc_audio_sender_sdp(
            webRtcAudioPeer->peerId, text, "answer");
    gst_webrtc_session_description_free(answer);
}

void on_incoming_stream_audio(GstElement *webrtc, GstPad *pad, WebRtcPeer *webRtcAudioPeer) {
    GST_INFO("Triggered on_incoming_stream ");
    if (GST_PAD_DIRECTION (pad) != GST_PAD_SRC) {
        GST_ERROR("Incorrect pad direction");
        return;
    }

    GstCaps *caps;
    const gchar *name;
    gchar *dynamic_pad_name;
    GstElement *rtpopusdepay;
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
    tmp = g_strdup_printf("rtpopusdepay_send-%s", webRtcAudioPeer->peerId.c_str());
    rtpopusdepay = gst_bin_get_by_name(
            GST_BIN (push2talkUtils::fetch_audio_pipelinehandler_by_key(webRtcAudioPeer->channelId)->pipeline), tmp);
    g_free(tmp);
    if (gst_element_link_pads(webrtc, dynamic_pad_name, rtpopusdepay, "sink")) {
        GST_INFO("webrtc pad %s linked to rtpopusdepay_send-x", dynamic_pad_name);
        gst_object_unref(rtpopusdepay);
        g_free(dynamic_pad_name);
        return;
    }
    return;
}

void send_sdp_offer_audio(GstWebRTCSessionDescription *offer, WebRtcPeer *webRtcAudioPeer) {
    gchar *text;

    text = gst_sdp_message_as_text(offer->sdp);
    GST_INFO("Sending offer for peer: %s \n %s", webRtcAudioPeer->peerId.c_str(), text);

    push2talkUtils::fetch_audio_pipelinehandler_by_key(webRtcAudioPeer->channelId)->send_webrtc_audio_receiver_sdp(
            webRtcAudioPeer->peerId, text, "offer");
    g_free(text);
}

/* Offer created by our audio pipeline, to be sent to the peer */
void on_offer_created_audio(GstPromise *promise, gpointer user_data) {
    GstWebRTCSessionDescription *offer = NULL;
    const GstStructure *reply;
    WebRtcPeer *webRtcAudioPeer = static_cast<WebRtcPeer *>(user_data);

    g_assert_cmphex (gst_promise_wait(promise), ==, GST_PROMISE_RESULT_REPLIED);
    reply = gst_promise_get_reply(promise);
    gst_structure_get(reply, "offer",
                      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);
    gst_promise_unref(promise);
    promise = gst_promise_new();
    g_signal_emit_by_name(webRtcAudioPeer->webrtcElement, "set-local-description", offer, promise);
    gst_promise_interrupt(promise);
    gst_promise_unref(promise);

    /* Send offer to peer */
    send_sdp_offer_audio(offer, webRtcAudioPeer);
    gst_webrtc_session_description_free(offer);
}

void on_negotiation_needed_audio(GstElement *element, WebRtcPeer *user_data) {
    GstPromise *promise;
    promise = gst_promise_new_with_change_func(on_offer_created_audio, user_data, NULL);
    g_signal_emit_by_name(user_data->webrtcElement, "create-offer", NULL, promise);
}

void AudioPipelineHandler::add_webrtc_audio_sender_receiver(std::string peerId) {
    GST_INFO("Adding sender and receiver peer %s for meetingId %s ", peerId.c_str(), channelId.c_str());

    //Creating sender
    WebRtcPeerPtr webRtcAudioPeerPtrSend = std::make_shared<WebRtcPeer>();
    webRtcAudioPeerPtrSend->peerDirection = SENDER;
    webRtcAudioPeerPtrSend->peerId = peerId;
    webRtcAudioPeerPtrSend->channelId = channelId;
    peerAudioSenders[peerId] = webRtcAudioPeerPtrSend;

    int retVal;
    GstElement *rtpopusdepay, *opusdec, *audiomixer;
    GstPad *srcpad, *sinkpad;
    /* Create Elements */
    gchar *tmp;
    tmp = g_strdup_printf("webrtcbin_send-%s", peerId.c_str());
    webRtcAudioPeerPtrSend->webrtcElement = gst_element_factory_make("webrtcbin", tmp);
    g_object_set(webRtcAudioPeerPtrSend->webrtcElement, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE, NULL);
    g_free(tmp);
    tmp = g_strdup_printf("rtpopusdepay_send-%s", peerId.c_str());
    rtpopusdepay = gst_element_factory_make("rtpopusdepay", tmp);
    g_free(tmp);
    tmp = g_strdup_printf("opusdec_send-%s", peerId.c_str());
    opusdec = gst_element_factory_make("opusdec", tmp);
    g_free(tmp);

    //Add elements to pipeline
    gst_bin_add_many(GST_BIN (pipeline), webRtcAudioPeerPtrSend->webrtcElement, rtpopusdepay, opusdec, NULL);

    if (!gst_element_link_many(rtpopusdepay, opusdec, NULL)) {
        GST_ERROR("add_webrtc_audio_sender_receiver: Error linking rtpopusdepay to opusdec for peer %s ",
                  peerId.c_str());
        return;
    }
    g_assert_nonnull (webRtcAudioPeerPtrSend->webrtcElement);
    g_signal_connect (webRtcAudioPeerPtrSend->webrtcElement, "on-ice-candidate",
                      G_CALLBACK(send_ice_candidate_message_audio), webRtcAudioPeerPtrSend.get());
    /* Incoming streams will be exposed via this signal */
    g_signal_connect (webRtcAudioPeerPtrSend->webrtcElement, "pad-added", G_CALLBACK(on_incoming_stream_audio),
                      webRtcAudioPeerPtrSend.get());

    //Link sender audio to mixer
    audiomixer = gst_bin_get_by_name(GST_BIN (pipeline), "audiomixer0");
    g_assert_nonnull (audiomixer);
    srcpad = gst_element_get_static_pad(opusdec, "src");
    g_assert_nonnull (srcpad);
    sinkpad = gst_element_get_request_pad(audiomixer, "sink_%u");
    g_assert_nonnull (sinkpad);
    retVal = gst_pad_link(srcpad, sinkpad);
    g_assert_cmpint (retVal, ==, GST_PAD_LINK_OK);
    gst_object_unref(srcpad);
    gst_object_unref(sinkpad);

    //Sync all created elements to pipeline
    retVal = gst_element_sync_state_with_parent(webRtcAudioPeerPtrSend->webrtcElement);
    g_assert_true (retVal);
    retVal = gst_element_sync_state_with_parent(rtpopusdepay);
    g_assert_true (retVal);
    retVal = gst_element_sync_state_with_parent(opusdec);
    g_assert_true (retVal);

    GST_INFO("Created webrtc bin for sender audio peer %s", peerId.c_str());

    //Creating receiver ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    WebRtcPeerPtr webRtcAudioPeerPtrRecv = std::make_shared<WebRtcPeer>();
    webRtcAudioPeerPtrRecv->peerDirection = RECEIVER;
    webRtcAudioPeerPtrRecv->peerId = peerId;
    webRtcAudioPeerPtrRecv->channelId = channelId;
    peerAudioReceivers[peerId] = webRtcAudioPeerPtrRecv;

    GstElement *audiotee, *queue, *opusenc, *rtpopuspay;
    /* Create Elements */
    tmp = g_strdup_printf("webrtcbin_recv-%s", peerId.c_str());
    webRtcAudioPeerPtrRecv->webrtcElement = gst_element_factory_make("webrtcbin", tmp);
    g_object_set(webRtcAudioPeerPtrRecv->webrtcElement, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE, NULL);
    g_free(tmp);
    tmp = g_strdup_printf("queue_recv-%s", peerId.c_str());
    queue = gst_element_factory_make("queue", tmp);
    g_object_set(queue, "leaky", 2, NULL);
    g_free(tmp);
    tmp = g_strdup_printf("opusenc_recv-%s", peerId.c_str());
    opusenc = gst_element_factory_make("opusenc", tmp);
    g_free(tmp);
    tmp = g_strdup_printf("rtpopuspay_recv-%s", peerId.c_str());
    rtpopuspay = gst_element_factory_make("rtpopuspay", tmp);
    g_free(tmp);


    //Add elements to pipeline
    gst_bin_add_many(GST_BIN (pipeline), webRtcAudioPeerPtrRecv->webrtcElement, queue, rtpopuspay, opusenc, NULL);

    //Link queue -> opusenc
    srcpad = gst_element_get_static_pad(queue, "src");
    g_assert_nonnull (srcpad);
    sinkpad = gst_element_get_static_pad(opusenc, "sink");
    g_assert_nonnull (sinkpad);
    retVal = gst_pad_link(srcpad, sinkpad);
    g_assert_cmpint (retVal, ==, GST_PAD_LINK_OK);
    gst_object_unref(srcpad);
    gst_object_unref(sinkpad);

    //Link opusenc -> rtpopuspay
    srcpad = gst_element_get_static_pad(opusenc, "src");
    g_assert_nonnull (srcpad);
    sinkpad = gst_element_get_static_pad(rtpopuspay, "sink");
    g_assert_nonnull (sinkpad);
    retVal = gst_pad_link(srcpad, sinkpad);
    g_assert_cmpint (retVal, ==, GST_PAD_LINK_OK);
    gst_object_unref(srcpad);
    gst_object_unref(sinkpad);

    //Link rtpopuspay -> webrtcbin
    srcpad = gst_element_get_static_pad(rtpopuspay, "src");
    g_assert_nonnull (srcpad);
    sinkpad = gst_element_get_request_pad(webRtcAudioPeerPtrRecv->webrtcElement, "sink_%u");
    g_assert_nonnull (sinkpad);
    retVal = gst_pad_link(srcpad, sinkpad);
    g_assert_cmpint (retVal, ==, GST_PAD_LINK_OK);
    gst_object_unref(srcpad);
    gst_object_unref(sinkpad);

    //Link audiotee -> queue
    audiotee = gst_bin_get_by_name(GST_BIN (pipeline), "audiotee0");
    g_assert_nonnull (audiotee);
    srcpad = gst_element_get_request_pad(audiotee, "src_%u");
    g_assert_nonnull (srcpad);
    gst_object_unref(audiotee);
    sinkpad = gst_element_get_static_pad(queue, "sink");
    g_assert_nonnull (sinkpad);
    retVal = gst_pad_link(srcpad, sinkpad);
    g_assert_cmpint (retVal, ==, GST_PAD_LINK_OK);
    gst_object_unref(srcpad);
    gst_object_unref(sinkpad);

    GstWebRTCRTPTransceiver *trans;
    GArray *transceivers;
    //Change webrtcbin to send only
    g_signal_emit_by_name(webRtcAudioPeerPtrRecv->webrtcElement, "get-transceivers", &transceivers);
    if (transceivers) {
        GST_INFO("Changing webrtcbin for receiver audiio peer %s to sendonly. Number of transceivers(%d) ",
                 peerId.c_str(),
                 transceivers->len);
        g_assert_true(transceivers->len > 0);
        trans = g_array_index (transceivers, GstWebRTCRTPTransceiver *, 0);
        trans->direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
        //g_object_set(trans, "fec-type", GST_WEBRTC_FEC_TYPE_ULP_RED, NULL);
        //g_object_set(trans, "do-nack", TRUE, NULL);
        g_array_unref(transceivers);
    }

    g_assert_nonnull (webRtcAudioPeerPtrRecv->webrtcElement);
    /* This is the gstwebrtc entry point where we create the offer and so on. It
     * will be called when the pipeline goes to PLAYING. */
    g_signal_connect (webRtcAudioPeerPtrRecv->webrtcElement, "on-negotiation-needed",
                      G_CALLBACK(on_negotiation_needed_audio), webRtcAudioPeerPtrRecv.get());
    /* We need to transmit this ICE candidate to the browser via the
     * signalling server. Incoming ice candidates from the browser need to be
     * added by us too*/
    g_signal_connect (webRtcAudioPeerPtrRecv->webrtcElement, "on-ice-candidate",
                      G_CALLBACK(send_ice_candidate_message_audio), webRtcAudioPeerPtrRecv.get());

    /* Incoming streams will be exposed via this signal */
    //g_signal_connect (webRtcAudioPeerPtrRecv->webrtcAudio, "pad-added", G_CALLBACK(on_incoming_stream), pipeline);
    retVal = gst_element_sync_state_with_parent(queue);
    g_assert_true (retVal);
    retVal = gst_element_sync_state_with_parent(opusenc);
    g_assert_true (retVal);
    retVal = gst_element_sync_state_with_parent(rtpopuspay);
    g_assert_true (retVal);
    retVal = gst_element_sync_state_with_parent(webRtcAudioPeerPtrRecv->webrtcElement);
    g_assert_true (retVal);
    GST_INFO("Created webrtc bin for receiver audio peer %s", peerId.c_str());

}

void AudioPipelineHandler::remove_webrtc_audio_sender_receiver(std::string peerId) {
    gchar *tmp;
    GstPad *srcpad, *sinkpad;
    GstElement *webrtcsender, *rtpopusdepay, *opusdec, *audiomixer;

    GST_INFO("remove_webrtc_audio_sender_receiver - peer %s ", peerId.c_str());

    //Removing sender peer
    tmp = g_strdup_printf("webrtcbin_send-%s", peerId.c_str());
    webrtcsender = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    if (webrtcsender) {
        GST_INFO("Removing existing sender webrtcbin for remote peer %s ", peerId.c_str());
        gst_element_set_state(webrtcsender, GST_STATE_NULL);
        gst_bin_remove(GST_BIN (pipeline), webrtcsender);
        gst_object_unref(webrtcsender);
    } else {
        GST_ERROR("remove_webrtc_audio_sender_receiver for remote sender peer %s - No existing sender webrtcbin found",
                  peerId.c_str());
        return;
    }

    tmp = g_strdup_printf("rtpopusdepay_send-%s", peerId.c_str());
    rtpopusdepay = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    g_free(tmp);
    if (rtpopusdepay) {
        gst_element_set_state(rtpopusdepay, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(pipeline), rtpopusdepay);
        gst_object_unref(rtpopusdepay);
    }

    tmp = g_strdup_printf("opusdec_send-%s", peerId.c_str());
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

        audiomixer = gst_bin_get_by_name(GST_BIN (pipeline), "audiomixer0");
        if (audiomixer) {
            g_assert_nonnull (audiomixer);
            gst_element_release_request_pad(audiomixer, sinkpad);
            gst_object_unref(sinkpad);
            gst_object_unref(audiomixer);
        }
    }
    GST_INFO("Removed sender peer webrtcbin peer : %s", peerId.c_str());

    //Removing receiver peer ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    GstElement *webrtcreciver, *rtpopuspay, *opusenc, *queue, *tee;

    tmp = g_strdup_printf("webrtcbin_recv-%s", peerId.c_str());
    webrtcreciver = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    if (webrtcreciver) {
        GST_INFO("Removing existing receiver webrtcbin for peer %s ", peerId.c_str());
        gst_element_set_state(webrtcreciver, GST_STATE_NULL);
        gst_bin_remove(GST_BIN (pipeline), webrtcreciver);
        gst_object_unref(webrtcreciver);
    } else {
        GST_ERROR("remove_peer_from_pipeline for peer %s - No existing receiver webrtcbin found",
                  peerId.c_str());
        return;
    }

    tmp = g_strdup_printf("opusenc_recv-%s", peerId.c_str());
    opusenc = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    g_free(tmp);
    if (opusenc) {
        gst_element_set_state(opusenc, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(pipeline), opusenc);
        gst_object_unref(opusenc);
    }

    tmp = g_strdup_printf("rtpopuspay_recv-%s", peerId.c_str());
    rtpopuspay = gst_bin_get_by_name(GST_BIN (pipeline), tmp);
    g_free(tmp);
    if (rtpopuspay) {
        gst_element_set_state(rtpopuspay, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(pipeline), rtpopuspay);
        gst_object_unref(rtpopuspay);
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

        tee = gst_bin_get_by_name(GST_BIN (pipeline), "audiotee0");
        if (tee) {
            g_assert_nonnull (tee);
            gst_element_release_request_pad(tee, srcpad);
            gst_object_unref(srcpad);
            gst_object_unref(tee);
        }
    }
    GST_INFO("Removed receiver webrtcbin peer : %s", peerId.c_str());

    remove_audio_sender_by_peerid(peerId);
    remove_audio_reciever_by_peerid(peerId);

    PeerMessageRequest peerMessageRequest;
    peerMessageRequest.mutable_peerstatusmessage()->set_status(PeerStatusMessage_Status_AUDIO_RESET);
    peerMessageRequest.set_peerid(peerId);
    peerMessageRequest.set_channelid(channelId);
    push2talkUtils::pushToTalkServiceClientPtr->sendPeerMessage(peerMessageRequest);

}

void AudioPipelineHandler::apply_webrtc_audio_sender_sdp(std::string peerId, std::string sdp, std::string type) {
    GST_INFO("Received SDP type %s ", type.c_str());
    g_assert_cmpstr (type.c_str(), ==, "offer");
    int ret;
    GstSDPMessage *sdpMsg;
    const gchar *text;
    GstWebRTCSessionDescription *offer;
    text = sdp.c_str();
    GST_INFO("Received offer:\n%s", text);

    ret = gst_sdp_message_new(&sdpMsg);
    g_assert_cmphex (ret, ==, GST_SDP_OK);

    ret = gst_sdp_message_parse_buffer((guint8 *) text, strlen(text), sdpMsg);
    g_assert_cmphex (ret, ==, GST_SDP_OK);

    offer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_OFFER, sdpMsg);
    g_assert_nonnull (offer);

    /* Set remote description on our pipeline */
    {
        GstPromise *promise = gst_promise_new();
        g_signal_emit_by_name(fetch_audio_sender_by_peerid(peerId)->webrtcElement, "set-remote-description", offer,
                              promise);
        gst_promise_interrupt(promise);
        gst_promise_unref(promise);

        /* Create an answer that we will send back to the peer */
        promise = gst_promise_new_with_change_func((GstPromiseChangeFunc) on_answer_created_audio,
                                                   (gpointer) fetch_audio_sender_by_peerid(peerId).get(), NULL);
        g_signal_emit_by_name(fetch_audio_sender_by_peerid(peerId)->webrtcElement, "create-answer", NULL, promise);
    }
}

void
AudioPipelineHandler::apply_webrtc_audio_receiver_sdp(std::string peerId, std::string sdp, std::string type) {
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
        remove_webrtc_audio_sender_receiver(peerId);
        return;
    }
    g_assert_cmpstr (sdptype, ==, "answer");

    GST_INFO("Received answer for receiver peer %s:\n%s", peerId.c_str(), sdp.c_str());

    ret = gst_sdp_message_new(&sdpMessage);
    g_assert_cmphex (ret, ==, GST_SDP_OK);

    ret = gst_sdp_message_parse_buffer((guint8 *) text, strlen(text), sdpMessage);
    if (ret != GST_SDP_OK) {
        GST_ERROR("Invalid SDP for receiver peer %s:\n%s", peerId.c_str(), sdp.c_str());
        remove_webrtc_audio_sender_receiver(peerId);
        return;
    }
    g_assert_cmphex (ret, ==, GST_SDP_OK);

    answer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER,
                                                sdpMessage);
    g_assert_nonnull (answer);

    /* Set remote description on our pipeline */
    {
        GstPromise *promise = gst_promise_new();
        g_signal_emit_by_name(fetch_audio_reciever_by_peerid(peerId)->webrtcElement, "set-remote-description", answer,
                              promise);
        gst_promise_interrupt(promise);
        gst_promise_unref(promise);
    }
    GST_INFO("apply_webrtc_audio_receiver_sdp completed for receiver peer %s ", peerId.c_str());
}

void AudioPipelineHandler::apply_webrtc_audio_sender_ice(std::string peerId, std::string ice, std::string mLineIndex) {
    int mlineindex = std::stoi(mLineIndex);
    g_signal_emit_by_name(fetch_audio_sender_by_peerid(peerId)->webrtcElement, "add-ice-candidate", mlineindex,
                          ice.c_str());
}

void
AudioPipelineHandler::apply_webrtc_audio_receiver_ice(std::string peerId, std::string ice, std::string mLineIndex) {
    const gchar *candidateMsg;
    gint sdpmlineindex;

    candidateMsg = ice.c_str();
    sdpmlineindex = stoi(mLineIndex);
    GST_INFO("Received ice for receiver peer %s:\n sdpmlineindex: %d \n %s", peerId.c_str(),
             sdpmlineindex, candidateMsg);

    /* Add ice candidateMsg sent by remote peer */
    g_signal_emit_by_name(fetch_audio_reciever_by_peerid(peerId)->webrtcElement, "add-ice-candidate", sdpmlineindex,
                          candidateMsg);
}

void AudioPipelineHandler::send_webrtc_audio_sender_sdp(std::string peerId, std::string sdp, std::string type) {
    PeerMessageRequest peerMessageRequest;
    peerMessageRequest.mutable_sdpmessage()->set_sdp(sdp);
    peerMessageRequest.mutable_sdpmessage()->set_type(type);
    peerMessageRequest.mutable_sdpmessage()->set_direction(SdpMessage_Direction_SENDER);
    peerMessageRequest.mutable_sdpmessage()->set_mediatype(SdpMessage_MediaType_AUDIO);
    peerMessageRequest.mutable_sdpmessage()->set_endpoint(SdpMessage_Endpoint_SERVER);
    peerMessageRequest.set_peerid(peerId);
    peerMessageRequest.set_channelid(channelId);
    push2talkUtils::pushToTalkServiceClientPtr->sendPeerMessage(peerMessageRequest);
}

void AudioPipelineHandler::send_webrtc_audio_receiver_sdp(std::string peerId, std::string sdp, std::string type) {
    PeerMessageRequest peerMessageRequest;
    peerMessageRequest.mutable_sdpmessage()->set_sdp(sdp);
    peerMessageRequest.mutable_sdpmessage()->set_type(type);
    peerMessageRequest.mutable_sdpmessage()->set_direction(SdpMessage_Direction_RECEIVER);
    peerMessageRequest.mutable_sdpmessage()->set_mediatype(SdpMessage_MediaType_AUDIO);
    peerMessageRequest.mutable_sdpmessage()->set_endpoint(SdpMessage_Endpoint_SERVER);
    peerMessageRequest.set_peerid(peerId);
    peerMessageRequest.set_channelid(channelId);
    push2talkUtils::pushToTalkServiceClientPtr->sendPeerMessage(peerMessageRequest);
}

void AudioPipelineHandler::send_webrtc_audio_sender_ice(std::string peerId, std::string ice, std::string mLineIndex) {
    PeerMessageRequest peerMessageRequest;
    peerMessageRequest.mutable_icemessage()->set_ice(ice);
    peerMessageRequest.mutable_icemessage()->set_mlineindex(mLineIndex);
    peerMessageRequest.mutable_icemessage()->set_direction(IceMessage_Direction_SENDER);
    peerMessageRequest.mutable_icemessage()->set_mediatype(IceMessage_MediaType_AUDIO);
    peerMessageRequest.mutable_icemessage()->set_endpoint(IceMessage_Endpoint_SERVER);
    peerMessageRequest.set_peerid(peerId);
    peerMessageRequest.set_channelid(channelId);
    push2talkUtils::pushToTalkServiceClientPtr->sendPeerMessage(peerMessageRequest);
}

void AudioPipelineHandler::send_webrtc_audio_receiver_ice(std::string peerId, std::string ice, std::string mLineIndex) {
    PeerMessageRequest peerMessageRequest;
    peerMessageRequest.mutable_icemessage()->set_ice(ice);
    peerMessageRequest.mutable_icemessage()->set_mlineindex(mLineIndex);
    peerMessageRequest.mutable_icemessage()->set_direction(IceMessage_Direction_RECEIVER);
    peerMessageRequest.mutable_icemessage()->set_mediatype(IceMessage_MediaType_AUDIO);
    peerMessageRequest.mutable_icemessage()->set_endpoint(IceMessage_Endpoint_SERVER);
    peerMessageRequest.set_peerid(peerId);
    peerMessageRequest.set_channelid(channelId);
    push2talkUtils::pushToTalkServiceClientPtr->sendPeerMessage(peerMessageRequest);
}