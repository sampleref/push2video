//
// Created by chakra on 23-01-2019.
//

#ifndef GSTPUSH2TALKSERVICE_GRPCSERVICE_HPP
#define GSTPUSH2TALKSERVICE_GRPCSERVICE_HPP

#ifndef GRPCCPP_SERVER_H_HEADER
#define GRPCCPP_SERVER_H_HEADER

#include <grpcpp/server.h>
#include "push2talk_service.grpc.pb.h"
#include "push2talk_service.pb.h"
#include <grpc++/grpc++.h>

#endif

class GrpcServer {
public:
    //Attributes
    std::unique_ptr<grpc::Server> server;

    //Methods
    void startServer(void);

    bool stopServer(void);

};

class PushToTalkServiceClient {
public:
    //Attributes
    std::unique_ptr<PushToTalk::Stub> push2talk_stub;

    PushToTalkServiceClient(std::shared_ptr<grpc::Channel> channel);

    void sendPeerMessage(PeerMessageRequest peerMessageRequest);

    void sendPing(std::string message);

    //Individual API's
    void sendSdpOfferRequest(SdpOfferRequest sdpOfferRequest);

    void sendSdpAnswerRequest(SdpAnswerRequest sdpAnswerRequest);

    void sendIceMessageRequest(IceMessageRequest iceMessageRequest);

    MgwFloorControlResponse sendMgwFloorControlRequest(MgwFloorControlRequest mgwFloorControlRequest);

};

typedef std::shared_ptr<PushToTalkServiceClient> PushToTalkServiceClientPtr;

#endif //GSTPUSH2TALKSERVICE_GRPCSERVICE_HPP
