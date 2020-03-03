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

#include "GrpcService.hpp"

GST_DEBUG_CATEGORY (push2talk_gst);
#define GST_CAT_DEFAULT push2talk_gst

class PushToTalkServiceImpl final : public PushToTalk::Service {

    ::grpc::Status sendPeerMessage(::grpc::ServerContext *context, const ::PeerMessageRequest *request,
                                   ::PeerMessageResponse *response) {

        return grpc::Status::OK;
    }
};

void GrpcServer::startServer() {
    GST_INFO("Starting grpc on port %u ", 12345);
    std::string server_address("0.0.0.0:12345");
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


