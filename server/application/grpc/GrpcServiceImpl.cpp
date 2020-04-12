//
// Created by chakra on 23-01-2019.
//

#include <gst/gst.h>
#include <stdio.h>
#include "thread"
#include <memory>

/* For GRPC */
#include <grpcpp/server_context.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

#include "GrpcService.hpp"
#include "../gstreamer/AudioPipelineHandler.hpp"
#include "../gstreamer/VideoPipelineHandler.hpp"
#include "../utils/Push2TalkUtils.hpp"

GST_DEBUG_CATEGORY_EXTERN (push2talk_gst);
#define GST_CAT_DEFAULT push2talk_gst

PushToTalkServiceClient::PushToTalkServiceClient(std::shared_ptr<grpc::Channel> channel) {
    push2talk_stub = PushToTalk::NewStub(channel);
}

PushToTalkServiceClientPtr createGrpcClient(std::string hostname_port) {
    return std::make_shared<PushToTalkServiceClient>(
            (grpc::CreateChannel(hostname_port, grpc::InsecureChannelCredentials())));
}

void PushToTalkServiceClient::sendPeerMessage(PeerMessageRequest peerMessageRequest) {
    ClientContext clientContext;
    PeerMessageResponse response;
    std::string messageInJson;
    google::protobuf::util::MessageToJsonString(peerMessageRequest, &messageInJson);
    GST_DEBUG("Sending message %s ", messageInJson.c_str());
    push2talk_stub->sendPeerMessage(&clientContext, peerMessageRequest, &response);
}

void PushToTalkServiceClient::sendPing(std::string message) {
    ClientContext clientContextPing;
    PingResponse pingResponse;
    PingRequest pingRequest;
    pingRequest.set_message(message);
    std::string messageInJson;
    google::protobuf::util::MessageToJsonString(pingRequest, &messageInJson);
    GST_INFO("Sending message %s ", messageInJson.c_str());
    if (push2talk_stub) {
        push2talk_stub->checkPing(&clientContextPing, pingRequest, &pingResponse);
    }
}

class PushToTalkServiceImpl final : public PushToTalk::Service {

    ::grpc::Status sendPeerMessage(::grpc::ServerContext *context, const ::PeerMessageRequest *request,
                                   ::PeerMessageResponse *response) {
        std::string messageInJson;
        google::protobuf::util::JsonOptions jsonOptions;
        jsonOptions.always_print_primitive_fields = true;
        jsonOptions.preserve_proto_field_names = true;
        google::protobuf::util::MessageToJsonString(*request, &messageInJson, jsonOptions);
        GST_INFO("Received message %s ", messageInJson.c_str());
        AudioPipelineHandlerPtr audioPipelineHandlerPtr = push2talkUtils::fetch_audio_pipelinehandler_by_key(
                request->channelid());
        VideoPipelineHandlerPtr videoPipelineHandlerPtr = push2talkUtils::fetch_video_pipelinehandler_by_key(
                request->channelid());
        if (request->has_peerstatusmessage()) {
            switch (request->peerstatusmessage().status()) {
                case PeerStatusMessage_Status_DISCONNECTED:
                    audioPipelineHandlerPtr->remove_webrtc_audio_sender_receiver(request->peerid());
                    if (videoPipelineHandlerPtr) {
                        videoPipelineHandlerPtr->remove_webrtc_video_peer(request->peerid());
                    }
                    break;
                case PeerStatusMessage_Status_AUDIO_RESET:
                    audioPipelineHandlerPtr->remove_webrtc_audio_sender_receiver(request->peerid());
                    break;
                case PeerStatusMessage_Status_VIDEO_RESET:
                    if (videoPipelineHandlerPtr) {
                        videoPipelineHandlerPtr->remove_webrtc_video_peer(request->peerid());
                    }
                    break;
                default:
                    GST_WARNING("peerstatusmessage Unhandled status message");
            }
        } else if (request->has_sdpmessage()) {
            if (request->sdpmessage().endpoint() == SdpMessage_Endpoint_SERVER) {
                GST_WARNING("sdpmessage Invalid Endpoint type");
            } else {
                switch (request->sdpmessage().direction()) {
                    case SdpMessage_Direction_SENDER:
                        switch (request->sdpmessage().mediatype()) {
                            case SdpMessage_MediaType_AUDIO:
                                //Start sender receiver webrtcbin's
                                audioPipelineHandlerPtr->add_webrtc_audio_sender_receiver(request->peerid());
                                audioPipelineHandlerPtr->apply_webrtc_audio_sender_sdp(request->peerid(),
                                                                                       request->sdpmessage().sdp(),
                                                                                       request->sdpmessage().type());
                                break;
                            case SdpMessage_MediaType_VIDEO: {
                                if (videoPipelineHandlerPtr == NULL) {
                                    videoPipelineHandlerPtr = std::make_shared<VideoPipelineHandler>();
                                    push2talkUtils::videoPipelineHandlers[request->channelid()] = videoPipelineHandlerPtr;
                                }
                                std::list<std::string> receivers;
                                for (std::string receiver : request->peersinchannel()) {
                                    //Skipping sender from receivers
                                    if (request->peerid().compare(receiver) != 0) {
                                        receivers.push_back(receiver);
                                    }
                                }
                                //Apply watchdog check to stop if not received media within timeout
                                videoPipelineHandlerPtr->apply_watchdog = APPLY_WATCHDOG_TIMEOUT;
                                videoPipelineHandlerPtr->create_video_pipeline_sender_peer(request->channelid(),
                                                                                           request->peerid(),
                                                                                           receivers,
                                                                                           request->sdpmessage().sdp(),
                                                                                           request->sdpmessage().type());
                                break;
                            }
                            default:
                                GST_WARNING("sdpmessage Unhandled media type");
                        }
                        break;
                    case SdpMessage_Direction_RECEIVER:
                        switch (request->sdpmessage().mediatype()) {
                            case SdpMessage_MediaType_AUDIO:
                                //Start sender receiver webrtcbin's
                                audioPipelineHandlerPtr->apply_webrtc_audio_receiver_sdp(request->peerid(),
                                                                                         request->sdpmessage().sdp(),
                                                                                         request->sdpmessage().type());
                                break;
                            case SdpMessage_MediaType_VIDEO: {
                                if (videoPipelineHandlerPtr) {
                                    videoPipelineHandlerPtr->apply_webrtc_video_receiver_sdp(request->peerid(),
                                                                                             request->sdpmessage().sdp(),
                                                                                             request->sdpmessage().type());
                                }
                                break;
                            }
                            default:
                                GST_WARNING("sdpmessage Unhandled media type");
                        }
                        break;
                    default:
                        GST_WARNING("sdpmessage Unhandled direction type");
                }
            }

        } else if (request->has_icemessage()) {
            if (request->icemessage().endpoint() == IceMessage_Endpoint_SERVER) {
                GST_WARNING("Invalid Endpoint type");
            } else {
                switch (request->icemessage().direction()) {
                    case IceMessage_Direction_SENDER:
                        switch (request->icemessage().mediatype()) {
                            case IceMessage_MediaType_AUDIO:
                                audioPipelineHandlerPtr->apply_webrtc_audio_sender_ice(request->peerid(),
                                                                                       request->icemessage().ice(),
                                                                                       request->icemessage().mlineindex());
                                break;
                            case IceMessage_MediaType_VIDEO: {
                                if (videoPipelineHandlerPtr) {
                                    videoPipelineHandlerPtr->apply_webrtc_video_sender_ice(request->peerid(),
                                                                                           request->icemessage().ice(),
                                                                                           request->icemessage().mlineindex());
                                }
                                break;
                            }
                            default:
                                GST_WARNING("icemessage Unhandled media type");
                        }
                        break;
                    case IceMessage_Direction_RECEIVER:
                        switch (request->icemessage().mediatype()) {
                            case IceMessage_MediaType_AUDIO:
                                audioPipelineHandlerPtr->apply_webrtc_audio_receiver_ice(request->peerid(),
                                                                                         request->icemessage().ice(),
                                                                                         request->icemessage().mlineindex());
                                break;
                            case IceMessage_MediaType_VIDEO: {
                                if (videoPipelineHandlerPtr) {
                                    videoPipelineHandlerPtr->apply_webrtc_video_receiver_ice(request->peerid(),
                                                                                             request->icemessage().ice(),
                                                                                             request->icemessage().mlineindex());
                                }
                                break;
                            }
                            default:
                                GST_WARNING("icemessage Unhandled media type");
                        }
                        break;
                    default:
                        GST_WARNING("sdpmessage Unhandled direction type");
                }
            }
        } else {
            GST_WARNING("Unhandled PeerMessageRequest");
        }
        return grpc::Status::OK;
    }

    ::grpc::Status createAudioChannel(::grpc::ServerContext *context, const ::CreateAudioChannelRequest *request,
                                      ::CreateAudioChannelResponse *response) {
        std::string messageInJson;
        google::protobuf::util::MessageToJsonString(*request, &messageInJson);
        GST_INFO("Received message %s ", messageInJson.c_str());
        AudioPipelineHandlerPtr audioPipelineHandlerPtr = std::make_shared<AudioPipelineHandler>();
        push2talkUtils::audioPipelineHandlers[request->channelid()] = audioPipelineHandlerPtr;
        audioPipelineHandlerPtr->channelId = request->channelid();
        audioPipelineHandlerPtr->start_pipeline();
        return grpc::Status::OK;
    }

    ::grpc::Status checkPing(::grpc::ServerContext *context, const ::PingRequest *request, ::PingResponse *response) {
        GST_INFO("Ping received ");
        response->set_message(request->message());
        return grpc::Status::OK;
    }

    ::grpc::Status signallingStarted(::grpc::ServerContext *context, const ::SignallingStart *request,
                                     ::google::protobuf::Empty *response) {
        GST_INFO("Signalling Started from '%s' ", request->hostnameport().c_str());
        push2talkUtils::pushToTalkServiceClientPtr = createGrpcClient(request->hostnameport());
        push2talkUtils::pushToTalkServiceClientPtr->sendPing("Hello Signalling, Welcome");
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
