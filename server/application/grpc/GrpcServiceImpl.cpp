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
#include "../gstreamer/GroupSendRecvHandler.hpp"
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

//Individual API's

void PushToTalkServiceClient::sendSdpOfferRequest(SdpOfferRequest sdpOfferRequest) {
    ClientContext clientContextSdpOffer;
    clientContextSdpOffer.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(5));
    SdpOfferResponse sdpOfferResponse;
    std::string messageInJson;
    google::protobuf::util::JsonOptions jsonOptions;
    jsonOptions.always_print_primitive_fields = true;
    jsonOptions.preserve_proto_field_names = true;
    google::protobuf::util::MessageToJsonString(sdpOfferRequest, &messageInJson, jsonOptions);
    GST_INFO("Sending message SdpOffer %s ", messageInJson.c_str());
    if (push2talk_stub) {
        push2talk_stub->sdpOffer(&clientContextSdpOffer, sdpOfferRequest, &sdpOfferResponse);
    }
    GST_INFO("Sent SDP Offer to %s ",
             UeMediaDirection_Direction_Name(sdpOfferRequest.uemediadirection().direction()).c_str());
}

void PushToTalkServiceClient::sendSdpAnswerRequest(SdpAnswerRequest sdpAnswerRequest) {
    ClientContext clientContextSdpAnswer;
    clientContextSdpAnswer.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(5));
    SdpAnswerResponse sdpAnswerResponse;
    std::string messageInJson;
    google::protobuf::util::JsonOptions jsonOptions;
    jsonOptions.always_print_primitive_fields = true;
    jsonOptions.preserve_proto_field_names = true;
    google::protobuf::util::MessageToJsonString(sdpAnswerRequest, &messageInJson, jsonOptions);
    GST_INFO("Sending message SdpAnswer %s ", messageInJson.c_str());
    if (push2talk_stub) {
        push2talk_stub->sdpAnswer(&clientContextSdpAnswer, sdpAnswerRequest, &sdpAnswerResponse);
    }
    GST_INFO("Sent SDP Answer Message to %s ",
             UeMediaDirection_Direction_Name(sdpAnswerRequest.uemediadirection().direction()).c_str());
}

void PushToTalkServiceClient::sendIceMessageRequest(IceMessageRequest iceMessageRequest) {
    ClientContext clientContextIceMessage;
    clientContextIceMessage.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(30));
    IceMessageResponse iceMessageResponse;
    std::string messageInJson;
    google::protobuf::util::JsonOptions jsonOptions;
    jsonOptions.always_print_primitive_fields = true;
    jsonOptions.preserve_proto_field_names = true;
    google::protobuf::util::MessageToJsonString(iceMessageRequest, &messageInJson, jsonOptions);
    GST_DEBUG("Sending message %s ", messageInJson.c_str());
    if (push2talk_stub) {
        push2talk_stub->iceMessage(&clientContextIceMessage, iceMessageRequest, &iceMessageResponse);
    }
}

MgwFloorControlResponse
PushToTalkServiceClient::sendMgwFloorControlRequest(MgwFloorControlRequest mgwFloorControlRequest) {
    ClientContext clientContextMgwFloorControl;
    MgwFloorControlResponse mgwFloorControlResponse;
    std::string messageInJson;
    google::protobuf::util::JsonOptions jsonOptions;
    jsonOptions.always_print_primitive_fields = true;
    jsonOptions.preserve_proto_field_names = true;
    google::protobuf::util::MessageToJsonString(mgwFloorControlRequest, &messageInJson, jsonOptions);
    GST_INFO("Sending message %s ", messageInJson.c_str());
    if (push2talk_stub) {
        push2talk_stub->mgwFloorControl(&clientContextMgwFloorControl, mgwFloorControlRequest,
                                        &mgwFloorControlResponse);
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
                    if (audioPipelineHandlerPtr) {
                        audioPipelineHandlerPtr->remove_webrtc_audio_sender_receiver(request->peerid());
                    }
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
                            case SdpMessage_MediaType_AUDIO: {
                                //Start sender receiver webrtcbin's
                                audioPipelineHandlerPtr->add_webrtc_audio_sender_receiver(request->peerid());
                                audioPipelineHandlerPtr->apply_webrtc_audio_sender_sdp(request->peerid(),
                                                                                       request->sdpmessage().sdp(),
                                                                                       request->sdpmessage().type());
                                break;
                            }
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
                            case IceMessage_MediaType_AUDIO: {
                                audioPipelineHandlerPtr->apply_webrtc_audio_sender_ice(request->peerid(),
                                                                                       request->icemessage().ice(),
                                                                                       request->icemessage().mlineindex());
                                break;
                            }
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

    ::grpc::Status
    createGroup(::grpc::ServerContext *context, const ::CreateGroupRequest *request, ::CreateGroupResponse *response) {
        GroupSendRecvHandlerPtr groupSendRecvHandlerPtr = push2talkUtils::fetch_groupsendrecvhandler_by_groupid(
                request->groupid());
        if (groupSendRecvHandlerPtr == NULL) {
            groupSendRecvHandlerPtr = std::make_shared<GroupSendRecvHandler>();
            push2talkUtils::groupSendRecvHandlers[request->groupid()] = groupSendRecvHandlerPtr;
            groupSendRecvHandlerPtr->groupId = request->groupid();
            groupSendRecvHandlerPtr->currentSenderUeId = ""; //Default
        }
        groupSendRecvHandlerPtr->create_group(request->groupid());
        return grpc::Status::OK;
    }

    ::grpc::Status
    deleteGroup(::grpc::ServerContext *context, const ::DeleteGroupRequest *request, ::DeleteGroupResponse *response) {
        GroupSendRecvHandlerPtr groupSendRecvHandlerPtr = push2talkUtils::fetch_groupsendrecvhandler_by_groupid(
                request->groupid());
        if (groupSendRecvHandlerPtr != NULL) {
            groupSendRecvHandlerPtr->close_pipeline_group();
        } else {
            GST_ERROR("No valid pipeline handler found for group %s ", request->groupid().c_str());
        }
        return grpc::Status::OK;
    }

    ::grpc::Status
    sdpOffer(::grpc::ServerContext *context, const ::SdpOfferRequest *request, ::SdpOfferResponse *response) {
        GroupSendRecvHandlerPtr groupSendRecvHandlerPtr = push2talkUtils::fetch_groupsendrecvhandler_by_groupid(
                request->groupid());
        if (groupSendRecvHandlerPtr != NULL) {
            groupSendRecvHandlerPtr->av_add_ue(request->ueid(), request->uemediadirection());
            groupSendRecvHandlerPtr->apply_webrtc_peer_sdp(request->ueid(), request->sdp(), "offer",
                                                           request->uemediadirection());
            UeMediaDirection direction;
            direction.set_direction(UeMediaDirection_Direction_RECEIVER);
            groupSendRecvHandlerPtr->av_add_ue(request->ueid(), direction);
        } else {
            GST_ERROR("No valid pipeline handler found for group %s ", request->groupid().c_str());
        }
        return grpc::Status::OK;
    }

    ::grpc::Status
    iceMessage(::grpc::ServerContext *context, const ::IceMessageRequest *request, ::IceMessageResponse *response) {
        GroupSendRecvHandlerPtr groupSendRecvHandlerPtr = push2talkUtils::fetch_groupsendrecvhandler_by_groupid(
                request->groupid());
        if (groupSendRecvHandlerPtr != NULL) {
            groupSendRecvHandlerPtr->ue_ice_message(request->ice(), request->mlineindex(), request->ueid(),
                                                    request->uemediadirection());
        } else {
            GST_ERROR("No valid pipeline handler found for group %s ", request->groupid().c_str());
        }
        return grpc::Status::OK;
    }

    ::grpc::Status
    sdpAnswer(::grpc::ServerContext *context, const ::SdpAnswerRequest *request, ::SdpAnswerResponse *response) {
        GroupSendRecvHandlerPtr groupSendRecvHandlerPtr = push2talkUtils::fetch_groupsendrecvhandler_by_groupid(
                request->groupid());
        if (groupSendRecvHandlerPtr != NULL) {
            groupSendRecvHandlerPtr->apply_incoming_sdp(request->ueid(), request->sdp(), "answer");
        } else {
            GST_ERROR("No valid pipeline handler found for group %s ", request->groupid().c_str());
        }
        return grpc::Status::OK;
    }

    ::grpc::Status ueFloorControl(::grpc::ServerContext *context, const ::UeFloorControlRequest *request,
                                  ::UeFloorControlResponse *response) {
        GroupSendRecvHandlerPtr groupSendRecvHandlerPtr = push2talkUtils::fetch_groupsendrecvhandler_by_groupid(
                request->groupid());
        if (groupSendRecvHandlerPtr != NULL) {
            UeFloorControlResponse_Action floorControlResponseAction = groupSendRecvHandlerPtr->update_ue_floor_control(
                    request->ueid(), request->action());
            response->set_ueid(request->ueid());
            response->set_groupid(request->groupid());
            response->set_action(floorControlResponseAction);
        } else {
            GST_ERROR("No valid pipeline handler found for group %s ", request->groupid().c_str());
        }
        return grpc::Status::OK;
    }

    ::grpc::Status
    ueReset(::grpc::ServerContext *context, const ::UeResetRequest *request, ::UeResetResponse *response) {
        GroupSendRecvHandlerPtr groupSendRecvHandlerPtr = push2talkUtils::fetch_groupsendrecvhandler_by_groupid(
                request->groupid());
        if (groupSendRecvHandlerPtr != NULL) {
            groupSendRecvHandlerPtr->reset_ue(request->ueid(), true, UeMediaDirection_Direction_SENDER);
            groupSendRecvHandlerPtr->reset_ue(request->ueid(), true, UeMediaDirection_Direction_RECEIVER);
        } else {
            GST_ERROR("No valid pipeline handler found for group %s ", request->groupid().c_str());
        }
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
