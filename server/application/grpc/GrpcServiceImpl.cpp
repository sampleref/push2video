//
// Created by chakra on 23-01-2019.
//

#include <gst/gst.h>
#include <stdio.h>
#include "thread"

/* For GRPC */
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/support/channel_arguments.h>

#include "GrpcService.hpp"
#include "../gstreamer/AudioPipelineHandler.hpp"
#include "../utils/Push2TalkUtils.hpp"

GST_DEBUG_CATEGORY_EXTERN (push2talk_gst);
#define GST_CAT_DEFAULT push2talk_gst

PushToTalkServiceClient::PushToTalkServiceClient(std::shared_ptr<grpc::Channel> channel) : stub_(
        PushToTalk::NewStub(channel)) {
}

PushToTalkServiceClientPtr createGrpcClient(std::string hostname_port) {
    grpc::ChannelArguments args;
    args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 3000);
    args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 3000);
    args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
    args.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);
    std::make_shared<PushToTalkServiceClient>(
            (grpc::CreateCustomChannel(hostname_port, grpc::InsecureChannelCredentials(), args)));
}

void PushToTalkServiceClient::sendPeerMessage(PeerMessageRequest peerMessageRequest) {
    grpc::ClientContext clientContext;
    PeerMessageResponse response;
    std::string messageInJson;
    google::protobuf::util::MessageToJsonString(peerMessageRequest, &messageInJson);
    GST_INFO("Sending message %s ", messageInJson.c_str());
    stub_->sendPeerMessage(&clientContext, peerMessageRequest, &response);
}

class PushToTalkServiceImpl final : public PushToTalk::Service {

    ::grpc::Status sendPeerMessage(::grpc::ServerContext *context, const ::PeerMessageRequest *request,
                                   ::PeerMessageResponse *response) {
        std::string messageInJson;
        google::protobuf::util::MessageToJsonString(*request, &messageInJson);
        GST_INFO("Received message %s ", messageInJson.c_str());
        AudioPipelineHandlerPtr audioPipelineHandlerPtr = push2talkUtils::fetch_audio_pipelinehandler_by_key(
                request->meetingid());
        if (request->has_peerstatusmessage()) {
            if (request->peerstatusmessage().status() == PeerStatusMessage_Status_DISCONNECTED ||
                request->peerstatusmessage().status() == PeerStatusMessage_Status_AUDIO_RESET) {
                audioPipelineHandlerPtr->remove_webrtc_audio_sender_receiver(request->peerid());
            } else {
                GST_WARNING("Unhandled status message");
            }
        } else if (request->has_sdpmessage()) {
            if (request->sdpmessage().endpoint() == SdpMessage_Endpoint_SERVER) {
                GST_WARNING("Invalid Endpoint type");
            } else {
                if (request->sdpmessage().direction() == SdpMessage_Direction_SENDER) {
                    //Start sender receiver webrtcbin's
                    audioPipelineHandlerPtr->add_webrtc_audio_sender_receiver(request->peerid());
                    audioPipelineHandlerPtr->apply_webrtc_audio_sender_sdp(request->peerid(),
                                                                           request->sdpmessage().sdp(),
                                                                           request->sdpmessage().type());
                } else if (request->sdpmessage().direction() == SdpMessage_Direction_RECEIVER) {
                    audioPipelineHandlerPtr->apply_webrtc_audio_receiver_sdp(request->peerid(),
                                                                             request->sdpmessage().sdp(),
                                                                             request->sdpmessage().type());
                } else {
                    GST_WARNING("Invalid Direction type");
                }
            }

        } else if (request->has_icemessage()) {
            if (request->icemessage().endpoint() == IceMessage_Endpoint_SERVER) {
                GST_WARNING("Invalid Endpoint type");
            } else {
                if (request->icemessage().direction() == IceMessage_Direction_SENDER) {
                    audioPipelineHandlerPtr->apply_webrtc_audio_sender_ice(request->peerid(),
                                                                           request->icemessage().ice(),
                                                                           request->icemessage().mlineindex());
                } else if (request->icemessage().direction() == IceMessage_Direction_RECEIVER) {
                    audioPipelineHandlerPtr->apply_webrtc_audio_receiver_ice(request->peerid(),
                                                                             request->icemessage().ice(),
                                                                             request->icemessage().mlineindex());
                } else {
                    GST_WARNING("Invalid Direction type");
                }
            }
        } else {
            GST_WARNING("Unhandled PeerMessageRequest");
        }

        return grpc::Status::OK;
    }

    ::grpc::Status createAudioMeeting(::grpc::ServerContext *context, const ::CreateAudioMeetingRequest *request,
                                      ::CreateAudioMeetingResponse *response) {
        std::string messageInJson;
        google::protobuf::util::MessageToJsonString(*request, &messageInJson);
        GST_INFO("Received message %s ", messageInJson.c_str());
        AudioPipelineHandlerPtr audioPipelineHandlerPtr = std::make_shared<AudioPipelineHandler>();
        audioPipelineHandlerPtr->meetingId = request->meetingid();
        audioPipelineHandlerPtr->start_pipeline();
        push2talkUtils::audioPipelineHandlers[request->meetingid()] = audioPipelineHandlerPtr;
        return grpc::Status::OK;
    }

    ::grpc::Status checkPing(::grpc::ServerContext *context, const ::PingRequest *request, ::PingResponse *response) {
        response->set_message(request->message());
        return grpc::Status::OK;
    }

    ::grpc::Status signallingStarted(::grpc::ServerContext *context, const ::SignallingStart *request,
                                     ::google::protobuf::Empty *response) {
        push2talkUtils::pushToTalkServiceClientPtr = createGrpcClient(request->hostnameport());
        return grpc::Status::OK;
    }
};

void GrpcServer::startServer() {
    GST_INFO("Starting grpc on port %u ", GRPC_SERVER_PORT);
    std::string server_address("0.0.0.0:" + to_string(GRPC_SERVER_PORT));
    PushToTalkServiceImpl service;

    grpc::ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    // Finally assemble the server.
    server = std::unique_ptr<grpc::Server>(builder.BuildAndStart());
    GST_INFO("Server listening on %s ", server_address.c_str());

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    server->Wait();
}

bool GrpcServer::stopServer() {
    GST_INFO("Stopping GRPC Server ");
    if (server != NULL) {
        server->Shutdown();
        GST_INFO("Stopped GRPC Server ");
    }
    return true;
}


