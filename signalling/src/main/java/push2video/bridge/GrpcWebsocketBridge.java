package push2video.bridge;

import com.push2talk.rpc.events.*;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;
import push2video.grpc.Push2VideoClient;
import push2video.utils.Constants;

import javax.websocket.EncodeException;
import javax.websocket.RemoteEndpoint;
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
        if (status.equals(PeerStatusMessage.Status.DISCONNECTED)) {
            push2VideoClient.sendUeDisconnectedMessage(peerId);
        }
    }

    public static boolean groupUeMessageToServer(GroupUeMessage groupUeMessage) {
        logger.info("Sending GroupUeMessage to server of type " + groupUeMessage.getMessageCase().name());
        try {
            switch (groupUeMessage.getMessageCase()) {
                case SDPOFFERREQUEST:
                    push2VideoClient.getGrpcClient().sdpOfferFromUe(groupUeMessage.getSdpOfferRequest());
                    break;
                case SDPANSWERREQUEST:
                    push2VideoClient.getGrpcClient().sdpAnswerFromUe(groupUeMessage.getSdpAnswerRequest());
                    break;
                case ICEMESSAGEREQUEST:
                    push2VideoClient.getGrpcClient().iceMessageFromUe(groupUeMessage.getIceMessageRequest());
                    break;
                case UEFLOORCONTROLREQUEST:
                    UeFloorControlResponse ueFloorControlResponse = push2VideoClient.getGrpcClient().ueFloorControl(groupUeMessage.getUeFloorControlRequest());
                    PeerMessageRequest peerMessageRequest = PeerMessageRequest.newBuilder().setGroupUeMessage(
                            GroupUeMessage.newBuilder().setUeFloorControlResponse(ueFloorControlResponse).build()
                    ).setPeerId(ueFloorControlResponse.getUeId()).setChannelId(ueFloorControlResponse.getGroupId())
                            .build();
                    sendToPeer(ueFloorControlResponse.getUeId(), peerMessageRequest);
                    break;
                case UERESETREQUEST:
                    push2VideoClient.getGrpcClient().ueReset(groupUeMessage.getUeResetRequest());
                    break;
                default:
            }
            return true;
        } catch (Exception e) {
            logger.error("Exception in groupUeMessageToServer ", e);
            return false;
        }
    }

    public static boolean sendToServer(String peerId, PeerMessageRequest peerMessageRequest) {
        logger.info("Sending message to server of type " + peerMessageRequest.getMessageCase().name());
        //Update all peers in channel list
        if (peerMessageRequest.getMessageCase().equals(PeerMessageRequest.MessageCase.SDPMESSAGE)
                && peerMessageRequest.getSdpMessage().getMediaType().equals(SdpMessage.MediaType.VIDEO)
                && peerMessageRequest.getSdpMessage().getDirection().equals(SdpMessage.Direction.SENDER)) {
            PeerMessageRequest.Builder peerMessageRequestOrBuilder = PeerMessageRequest.newBuilder();
            peerMessageRequestOrBuilder.setChannelId(peerMessageRequest.getChannelId());
            peerMessageRequestOrBuilder.setPeerId(peerMessageRequest.getPeerId());
            peerMessageRequestOrBuilder.setSdpMessage(SdpMessage.newBuilder()
                    .setDirection(SdpMessage.Direction.SENDER)
                    .setMediaType(SdpMessage.MediaType.VIDEO)
                    .setEndpoint(SdpMessage.Endpoint.CLIENT)
                    .setSdp(peerMessageRequest.getSdpMessage().getSdp())
                    .setType(peerMessageRequest.getSdpMessage().getType())
                    .build());
            peerMessageRequestOrBuilder.addAllPeersInChannel(peerChannelMap.get(Constants.DEFUALT_AUDIO_CHANNEL));
            peerMessageRequest = peerMessageRequestOrBuilder.build();
        }
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
            push2VideoClient.sendUeDisconnectedMessage(peerId);
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
                        if (peerMessageRequest.getSdpMessage().getMediaType().equals(SdpMessage.MediaType.AUDIO)) {
                            return true;
                        }
                        switch (peerMessageRequest.getSdpMessage().getDirection()) {
                            case SENDER:
                                logger.error("Already video locked! Another sender dropped");
                                return false;
                            default:
                                return true;
                        }
                    case PEERSTATUSMESSAGE:
                        if (peerMessageRequest.getPeerStatusMessage().getStatus().equals(PeerStatusMessage.Status.AUDIO_RESET)) {
                            return true;
                        }
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
                        && peerMessageRequest.getSdpMessage().getDirection().equals(SdpMessage.Direction.SENDER)
                        && peerMessageRequest.getSdpMessage().getMediaType().equals(SdpMessage.MediaType.VIDEO)) {
                    logger.error("Video locked ! Adding sender " + peerMessageRequest.getPeerId() + " for video lock");
                    videoLock = true;
                    videoLockPeer = peerMessageRequest.getPeerId();
                }
            }
        }
        return true;
    }

    public static boolean videoLock() {
        synchronized (videoLock) {
            return videoLock;
        }
    }

}
