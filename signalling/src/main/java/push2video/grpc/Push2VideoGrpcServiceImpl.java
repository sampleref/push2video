package push2video.grpc;

import com.push2talk.rpc.events.PushToTalkGrpc;
import org.apache.logging.log4j.LogManager;
import push2video.bridge.GrpcWebsocketBridge;

public class Push2VideoGrpcServiceImpl extends PushToTalkGrpc.PushToTalkImplBase {

    private static final org.apache.logging.log4j.Logger logger = LogManager.getLogger();

    public void signallingStarted(com.push2talk.rpc.events.SignallingStart request,
                                  io.grpc.stub.StreamObserver<com.google.protobuf.Empty> responseObserver) {
        //Not implemented on signalling end
    }

    /**
     */
    public void sendPeerMessage(com.push2talk.rpc.events.PeerMessageRequest request,
                                io.grpc.stub.StreamObserver<com.push2talk.rpc.events.PeerMessageResponse> responseObserver) {
        GrpcWebsocketBridge.sendToPeer(request.getPeerId(), request);
    }

    /**
     */
    public void createAudioMeeting(com.push2talk.rpc.events.CreateAudioMeetingRequest request,
                                   io.grpc.stub.StreamObserver<com.push2talk.rpc.events.CreateAudioMeetingResponse> responseObserver) {
        //Not implemented on signalling end
    }

    /**
     */
    public void checkPing(com.push2talk.rpc.events.PingRequest request,
                          io.grpc.stub.StreamObserver<com.push2talk.rpc.events.PingResponse> responseObserver) {
        //Not implemented on signalling end
    }

}
