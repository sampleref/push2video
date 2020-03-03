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

#endif

class GrpcServer {

public:
    //Attributes
    std::unique_ptr<grpc::Server> server;

    //Methods
    void startServer(void);

    bool stopServer(void);

};

#endif //GSTPUSH2TALKSERVICE_GRPCSERVICE_HPP
