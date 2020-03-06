package push2video.grpc;

import com.push2talk.rpc.events.*;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;
import push2video.utils.Constants;

public class Push2VideoClient {

    private static final Logger logger = LogManager.getLogger();

    private Push2VideoGrpcClientImpl push2VideoGrpcClient;

    public Push2VideoClient() throws Exception {
        push2VideoGrpcClient = Push2VideoGrpcClientImpl.getInstance("127.0.0.1", Constants.GRPC_SERVER_PORT);
    }

    public void updateSignallingStart(String hostname, int port) {
        push2VideoGrpcClient.signallingStarted("127.0.0.1", Constants.GRPC_CLIENT_PORT);
    }

    public boolean sendPeerStatusMessage(String peerId, PeerStatusMessage.Status status) {
        PeerMessageRequest peerMessageRequest = PeerMessageRequest.newBuilder()
                .setPeerStatusMessage(PeerStatusMessage.newBuilder().setStatus(status))
                .setPeerId(peerId)
                .setMeetingId(Constants.DEFUALT_AUDIO_MEETING).build();
        PeerMessageResponse peerMessageResponse = push2VideoGrpcClient.sendPeerMessage(peerMessageRequest);
        if (peerMessageResponse.getStatus().equals(PeerMessageResponse.Status.OK)) {
            return true;
        }
        return false;
    }

    public boolean sendPeerSdpIceMessages(String peerId, PeerMessageRequest peerMessageRequest) {
        PeerMessageResponse peerMessageResponse = push2VideoGrpcClient.sendPeerMessage(peerMessageRequest);
        if (peerMessageResponse.getStatus().equals(PeerMessageResponse.Status.OK)) {
            return true;
        }
        return false;
    }

    public void createAudioMeeting(String meetingId) {
        boolean success = push2VideoGrpcClient.createAudioMeeting(Constants.DEFUALT_AUDIO_MEETING);
        if (!success) {
            logger.error("Meeting cannot be created with default meeting id " + Constants.DEFUALT_AUDIO_MEETING);
        }
    }
}
