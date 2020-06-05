package push2video.grpc;

import com.google.protobuf.Empty;
import com.google.protobuf.util.JsonFormat;
import com.push2talk.rpc.events.*;
import org.apache.logging.log4j.LogManager;
import push2video.bridge.GrpcWebsocketBridge;

import static io.grpc.stub.ServerCalls.asyncUnimplementedUnaryCall;

public class Push2VideoGrpcServiceImpl extends PushToTalkGrpc.PushToTalkImplBase {

    private static final org.apache.logging.log4j.Logger logger = LogManager.getLogger();
    private static JsonFormat.Printer printer = JsonFormat.printer().includingDefaultValueFields();

    public void signallingStarted(com.push2talk.rpc.events.SignallingStart request,
                                  io.grpc.stub.StreamObserver<com.google.protobuf.Empty> responseObserver) {
        responseObserver.onNext(Empty.newBuilder().build());
        responseObserver.onCompleted();
    }

    /**
     *
     */
    public void sdpAnswer(com.push2talk.rpc.events.SdpAnswerRequest request,
                          io.grpc.stub.StreamObserver<com.push2talk.rpc.events.SdpAnswerResponse> responseObserver) {
        PeerMessageRequest peerMessageRequest = PeerMessageRequest.newBuilder()
                .setPeerId(request.getUeId())
                .setChannelId(request.getGroupId())
                .setGroupUeMessage(
                        GroupUeMessage.newBuilder().setSdpAnswerRequest(request).build()
                ).build();
        GrpcWebsocketBridge.sendToPeer(request.getUeId(), peerMessageRequest);
    }

    /**
     *
     */
    public void sdpOffer(com.push2talk.rpc.events.SdpOfferRequest request,
                         io.grpc.stub.StreamObserver<com.push2talk.rpc.events.SdpOfferResponse> responseObserver) {
        PeerMessageRequest peerMessageRequest = PeerMessageRequest.newBuilder()
                .setPeerId(request.getUeId())
                .setChannelId(request.getGroupId())
                .setGroupUeMessage(
                        GroupUeMessage.newBuilder().setSdpOfferRequest(request).build()
                ).build();
        GrpcWebsocketBridge.sendToPeer(request.getUeId(), peerMessageRequest);
    }

    /**
     *
     */
    public void iceMessage(com.push2talk.rpc.events.IceMessageRequest request,
                           io.grpc.stub.StreamObserver<com.push2talk.rpc.events.IceMessageResponse> responseObserver) {
        PeerMessageRequest peerMessageRequest = PeerMessageRequest.newBuilder()
                .setPeerId(request.getUeId())
                .setChannelId(request.getGroupId())
                .setGroupUeMessage(
                        GroupUeMessage.newBuilder().setIceMessageRequest(request).build()
                ).build();
        GrpcWebsocketBridge.sendToPeer(request.getUeId(), peerMessageRequest);
    }

    /**
     *
     */
    public void mgwFloorControl(com.push2talk.rpc.events.MgwFloorControlRequest request,
                                io.grpc.stub.StreamObserver<com.push2talk.rpc.events.MgwFloorControlResponse> responseObserver) {
        PeerMessageRequest peerMessageRequest = PeerMessageRequest.newBuilder()
                .setPeerId(request.getUeId())
                .setChannelId(request.getGroupId())
                .setGroupUeMessage(
                        GroupUeMessage.newBuilder().setMgwFloorControlRequest(request).build()
                ).build();
        GrpcWebsocketBridge.sendToPeer(request.getUeId(), peerMessageRequest);
    }

    /**
     *
     */
    public void sendPeerMessage(com.push2talk.rpc.events.PeerMessageRequest request,
                                io.grpc.stub.StreamObserver<com.push2talk.rpc.events.PeerMessageResponse> responseObserver) {
        try {
            logger.info("Peer Message from server: " + printer.print(request));
        } catch (Exception e) {
            logger.error(e);
        }
        if (request.getMessageCase().equals(PeerMessageRequest.MessageCase.PEERSTATUSMESSAGE)
                && request.getPeerStatusMessage().getStatus().equals(PeerStatusMessage.Status.VIDEO_RESET)) {
            logger.error("Video reset from server for sender peer " + request.getPeerId());
            GrpcWebsocketBridge.updateLockAndVerify(request);
        } else {
            GrpcWebsocketBridge.sendToPeer(request.getPeerId(), request);
        }
        responseObserver.onNext(PeerMessageResponse.newBuilder().setChannelId(request.getChannelId()).setPeerId(request.getPeerId()).setStatus(PeerMessageResponse.Status.OK).build());
        responseObserver.onCompleted();
    }

    /**
     *
     */
    public void createAudioChannel(com.push2talk.rpc.events.CreateAudioChannelRequest request,
                                   io.grpc.stub.StreamObserver<com.push2talk.rpc.events.CreateAudioChannelResponse> responseObserver) {
        responseObserver.onNext(CreateAudioChannelResponse.newBuilder().setChannelId(request.getChannelId()).setStatus(CreateAudioChannelResponse.Status.OK).build());
        responseObserver.onCompleted();
    }

    /**
     *
     */
    public void checkPing(com.push2talk.rpc.events.PingRequest request,
                          io.grpc.stub.StreamObserver<com.push2talk.rpc.events.PingResponse> responseObserver) {
        logger.info("Check Ping received from server");
        responseObserver.onNext(PingResponse.newBuilder().setMessage(request.getMessage()).build());
        responseObserver.onCompleted();
    }

}
