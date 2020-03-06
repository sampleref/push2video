package push2video.grpc;

import com.push2talk.rpc.events.*;
import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import static io.grpc.stub.ServerCalls.asyncUnimplementedUnaryCall;


public class Push2VideoGrpcClientImpl {

    private static final Logger logger = LogManager.getLogger();
    private static Push2VideoGrpcClientImpl instance;

    // Static variables
    private static ManagedChannel managedChannel;
    private static PushToTalkGrpc.PushToTalkBlockingStub pushToTalkBlockingStub;

    private Push2VideoGrpcClientImpl(String host, int port) throws Exception {
        pushToTalkBlockingStub = createConnection(host, port);
    }

    public static Push2VideoGrpcClientImpl getInstance(String host, int port) throws Exception {
        if (instance == null || managedChannel == null
                || managedChannel.isTerminated() || managedChannel.isShutdown()) {
            instance = new Push2VideoGrpcClientImpl(host, port);
        }
        return instance;
    }

    private PushToTalkGrpc.PushToTalkBlockingStub createConnection(String serverAddress, Integer port) throws Exception {
        logger.info("CreateConnection with server address: " + serverAddress + " port: " + port);
        try {
            managedChannel = ManagedChannelBuilder.forAddress(serverAddress, port).usePlaintext(true).build();
            return PushToTalkGrpc.newBlockingStub(managedChannel);
        } catch (Exception ex) {
            logger.error("Exception while creating grpc client " + ex.getMessage());
            throw ex;
        }
    }

    public void signallingStarted(String hostname, int port) {
        SignallingStart signallingStart = SignallingStart.newBuilder().setHostnamePort(hostname + ":" + Integer.toString(port)).build();
        pushToTalkBlockingStub.signallingStarted(signallingStart);
    }

    public PeerMessageResponse sendPeerMessage(com.push2talk.rpc.events.PeerMessageRequest request) {
        return pushToTalkBlockingStub.sendPeerMessage(request);
    }

    public boolean createAudioMeeting(String meetingId) {
        CreateAudioMeetingResponse createAudioMeetingResponse = pushToTalkBlockingStub.createAudioMeeting(CreateAudioMeetingRequest.newBuilder().setMeetingId(meetingId).build());
        if (createAudioMeetingResponse.getStatus().equals(CreateAudioMeetingResponse.Status.OK)) {
            return true;
        }
        return false;
    }

    /**
     *
     */
    public boolean checkPing() {
        PingResponse pingResponse = pushToTalkBlockingStub.checkPing(PingRequest.newBuilder().build());
        return true;
    }

}
