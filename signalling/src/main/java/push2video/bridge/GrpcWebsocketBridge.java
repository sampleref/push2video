package push2video.bridge;

import com.push2talk.rpc.events.PeerMessageRequest;
import com.push2talk.rpc.events.PeerStatusMessage;
import com.push2talk.rpc.events.SdpMessage;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;
import push2video.grpc.Push2VideoClient;
import push2video.utils.Constants;

import javax.websocket.EncodeException;
import javax.websocket.Session;
import java.io.IOException;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class GrpcWebsocketBridge {
    private static final Logger logger = LogManager.getLogger();

    public static Map<String, Session> peerSessionMap = Collections.synchronizedMap(new HashMap<>());
    public static Map<String, String> sessionPeerMap = Collections.synchronizedMap(new HashMap<>());
    public static Map<String, List<String>> peerChannelMap = Collections.synchronizedMap(new HashMap<>());
    private static Push2VideoClient push2VideoClient;

    //Video Channel Lock
    private static Boolean videoLock = false;
    private static String videoLockPeer = "";

    public static void initialize(Push2VideoClient push2VideoClient1) {
        push2VideoClient = push2VideoClient1;
    }

    public static void sendStatusToServer(String peerId, PeerStatusMessage.Status status) {
        push2VideoClient.sendPeerStatusMessage(peerId, status);
    }

    public static boolean sendToServer(String peerId, PeerMessageRequest peerMessageRequest) {
        //Update all peers in channel list
        peerMessageRequest.getPeersInChannelList().addAll(peerChannelMap.get(Constants.DEFUALT_AUDIO_CHANNEL));
        boolean success = push2VideoClient.sendPeerSdpIceMessages(peerId, peerMessageRequest);
        if (!success) {
            try {
                sendStatusMessageToPeer(peerId, PeerStatusMessage.Status.AUDIO_RESET);
                return false;
            } catch (Exception e) {
                logger.error(e);
                return false;
            }
        }
        return success;
    }

    public static boolean sendToPeer(String peerId, PeerMessageRequest peerMessageRequest) {
        try {
            sendMessageToPeer(peerMessageRequest);
            return true;
        } catch (Exception e) {
            logger.error(e);
            push2VideoClient.sendPeerStatusMessage(peerId, PeerStatusMessage.Status.DISCONNECTED);
            return false;
        }
    }

    public static void sendMessageToPeer(PeerMessageRequest peerMessageRequest) throws IOException, EncodeException {
        peerSessionMap.get(peerMessageRequest.getPeerId()).getBasicRemote().sendObject(peerMessageRequest);
    }

    public static void sendStatusMessageToPeer(String peerId, PeerStatusMessage.Status status) throws IOException, EncodeException {
        PeerMessageRequest peerMessageRequest = PeerMessageRequest.newBuilder()
                .setPeerStatusMessage(PeerStatusMessage.newBuilder()
                        .setStatus(status).build()).build();
        peerSessionMap.get(peerMessageRequest.getPeerId()).getBasicRemote().sendObject(peerMessageRequest);
    }

    public static boolean updateLockAndVerify(PeerMessageRequest peerMessageRequest) {
        synchronized (videoLock) {
            if (videoLock) {
                switch (peerMessageRequest.getMessageCase()) {
                    case SDPMESSAGE:
                        switch (peerMessageRequest.getSdpMessage().getDirection()) {
                            case SENDER:
                                logger.error("Already video locked! Another sender dropped");
                                return false;
                            default:
                                return true;
                        }
                    case PEERSTATUSMESSAGE:
                        switch (peerMessageRequest.getPeerStatusMessage().getStatus()) {
                            case VIDEO_RESET:
                            case DISCONNECTED:
                                if (videoLockPeer.equalsIgnoreCase(peerMessageRequest.getPeerId())) {
                                    logger.error("Video lock peer stopped/disconnected. Releasing lock");
                                    videoLock = false;
                                    videoLockPeer = "";
                                }
                                return true;
                            default:
                                return true;
                        }
                    default:
                        return true;
                }
            } else {
                if (peerMessageRequest.getMessageCase().equals(PeerMessageRequest.MessageCase.SDPMESSAGE)
                        && peerMessageRequest.getSdpMessage().getDirection().equals(SdpMessage.Direction.SENDER)) {
                    logger.error("No Video lock! Adding sender " + peerMessageRequest.getPeerId() + " for video lock");
                    videoLock = true;
                    videoLockPeer = peerMessageRequest.getPeerId();
                }
            }
        }
        return true;
    }

}
