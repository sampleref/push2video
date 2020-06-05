package push2video.grpc;

import com.push2talk.rpc.events.*;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;
import push2video.bridge.GrpcWebsocketBridge;
import push2video.utils.Constants;
import push2video.utils.Push2VideoUtils;

import java.util.ArrayList;

public class Push2VideoClient {

    private static final Logger logger = LogManager.getLogger();

    private Push2VideoGrpcClientImpl push2VideoGrpcClient;

    public Push2VideoClient() throws Exception {
        push2VideoGrpcClient = Push2VideoGrpcClientImpl.getInstance(Push2VideoUtils.getEnv(Constants.SERVER_HOST_KEY, Constants.SERVER_HOST_DEF), Constants.GRPC_SERVER_PORT);
    }

    public Push2VideoGrpcClientImpl getGrpcClient(){
        return push2VideoGrpcClient;
    }

    public void updateSignallingStart(String clientHostname, int clientPort) {
        push2VideoGrpcClient.checkPing();
        push2VideoGrpcClient.signallingStarted(clientHostname, clientPort);
    }

    public boolean sendPeerStatusMessage(String peerId, PeerStatusMessage.Status status) {
        PeerMessageRequest peerMessageRequest = PeerMessageRequest.newBuilder()
                .setPeerStatusMessage(PeerStatusMessage.newBuilder().setStatus(status))
                .setPeerId(peerId)
                .setChannelId(Constants.DEFUALT_AUDIO_CHANNEL).build();
        PeerMessageResponse peerMessageResponse = push2VideoGrpcClient.sendPeerMessage(peerMessageRequest);
        if (peerMessageResponse.getStatus().equals(PeerMessageResponse.Status.OK)) {
            return true;
        }
        return false;
    }

    public boolean sendUeDisconnectedMessage(String ueId) {
        UeResetRequest ueResetRequest = UeResetRequest.newBuilder().setUeId(ueId).setGroupId(Constants.DEFUALT_AV_GROUP).build();
        return push2VideoGrpcClient.ueReset(ueResetRequest);
    }

    public boolean sendPeerSdpIceMessages(String peerId, PeerMessageRequest peerMessageRequest) {
        PeerMessageResponse peerMessageResponse = push2VideoGrpcClient.sendPeerMessage(peerMessageRequest);
        if (peerMessageResponse.getStatus().equals(PeerMessageResponse.Status.OK)) {
            return true;
        }
        return false;
    }

    public void createAudioChannel(String channelId) {
        boolean success = push2VideoGrpcClient.createAudioChannel(channelId);
        if (!success) {
            logger.error("Channel cannot be created with channel id " + channelId);
            return;
        }
        GrpcWebsocketBridge.peerChannelMap.put(channelId, new ArrayList<>());
    }
}
