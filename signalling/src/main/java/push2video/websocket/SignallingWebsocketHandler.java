package push2video.websocket;

import com.push2talk.rpc.events.ChannelStatusMessage;
import com.push2talk.rpc.events.PeerMessageRequest;
import com.push2talk.rpc.events.PeerStatusMessage;
import org.apache.commons.lang3.StringUtils;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;
import push2video.bridge.GrpcWebsocketBridge;
import push2video.utils.Constants;
import push2video.utils.Push2VideoUtils;

import javax.websocket.*;
import javax.websocket.server.ServerEndpoint;
import java.io.IOException;

/**
 * @author chakra
 */
@ServerEndpoint(value = "/signalling", encoders = {MessageEncoder.class}, decoders = {MessageDecoder.class})
public class SignallingWebsocketHandler {

    private static final Logger logger = LogManager.getLogger(SignallingWebsocketHandler.class);

    @OnOpen
    public void onOpen(Session session, EndpointConfig endpointConfig) throws IOException, EncodeException {
        session.setMaxIdleTimeout(Constants.IDLE_WEBSOCKET_TIMEOUT);
        String peerId = sendAllocatedPeerIdWithConnected(session);
        if (StringUtils.isNotBlank(peerId)) {
            GrpcWebsocketBridge.peerSessionMap.put(peerId, session);
            GrpcWebsocketBridge.sessionPeerMap.put(session.getId(), peerId);
            GrpcWebsocketBridge.peerChannelMap.get(Constants.DEFUALT_AUDIO_CHANNEL).add(peerId);
        } else {
            logger.error("Peer Id cannot be allocated!");
        }
    }

    @OnMessage
    public void onMessage(PeerMessageRequest message, Session session) throws IOException, EncodeException {
        logger.debug("Message received from " + session.getId());
        switch (message.getMessageCase()) {
            case CHANNELSTATUSMESSAGE:
                sendVideoLockStatusToPeer(session);
                break;
            case GROUPUEMESSAGE:
                GrpcWebsocketBridge.groupUeMessageToServer(message.getGroupUeMessage());
                break;
            default:
                if (GrpcWebsocketBridge.updateLockAndVerify(message)) {
                    GrpcWebsocketBridge.sendToServer(GrpcWebsocketBridge.sessionPeerMap.get(session.getId()), message);
                }
        }
    }

    @OnClose
    public void onClose(Session session) throws IOException, EncodeException {
        logger.info("Websocket with session Id " + session.getId() + " is closed");
        PeerMessageRequest request = PeerMessageRequest.newBuilder().setPeerId(GrpcWebsocketBridge.sessionPeerMap.get(session.getId()))
                .setPeerStatusMessage(PeerStatusMessage.newBuilder().setStatus(PeerStatusMessage.Status.DISCONNECTED).build()).build();
        GrpcWebsocketBridge.updateLockAndVerify(request);
        GrpcWebsocketBridge.peerChannelMap.get(Constants.DEFUALT_AUDIO_CHANNEL).remove(GrpcWebsocketBridge.sessionPeerMap.get(session.getId()));
        GrpcWebsocketBridge.sendStatusToServer(GrpcWebsocketBridge.sessionPeerMap.get(session.getId()), PeerStatusMessage.Status.DISCONNECTED);
        GrpcWebsocketBridge.peerSessionMap.remove(GrpcWebsocketBridge.sessionPeerMap.get(session.getId()));
        GrpcWebsocketBridge.sessionPeerMap.remove(session.getId());
    }

    @OnError
    public void onError(Throwable t) {
        logger.error("onError::" + t.getMessage());
    }

    private static String sendAllocatedPeerIdWithConnected(Session session) {
        String randomString = Push2VideoUtils.generateRandomString();
        logger.info("Websocket opened with sessionId: " + session.getId() + " Assigned peerId: " + randomString);
        PeerMessageRequest peerMessageRequestConnected = PeerMessageRequest.newBuilder().setPeerId(randomString)
                .setPeerStatusMessage(PeerStatusMessage.newBuilder().setStatus(PeerStatusMessage.Status.CONNECTED).build())
                .build();
        try {
            session.getBasicRemote().sendObject(peerMessageRequestConnected);
        } catch (Exception e) {
            logger.error(e);
            return "";
        }
        return randomString;
    }

    private static void sendVideoLockStatusToPeer(Session session) {
        PeerMessageRequest.Builder peerMessageRequestVideoStateBuilder;
        String peerId = GrpcWebsocketBridge.sessionPeerMap.get(session.getId());
        if (StringUtils.isBlank(peerId)) {
            logger.error("Invalid peer for session: " + session.getId());
            return;
        }
        peerMessageRequestVideoStateBuilder = PeerMessageRequest.newBuilder();
        peerMessageRequestVideoStateBuilder.setPeerId(peerId);
        if (GrpcWebsocketBridge.videoLock()) {
            logger.info("Video lock true for peerId: " + peerId);
            peerMessageRequestVideoStateBuilder.setChannelStatusMessage(ChannelStatusMessage.newBuilder().setStatus(ChannelStatusMessage.Status.VIDEO_LOCKED).build());
        } else {
            logger.info("Video lock false for peerId: " + peerId);
            peerMessageRequestVideoStateBuilder.setChannelStatusMessage(ChannelStatusMessage.newBuilder().setStatus(ChannelStatusMessage.Status.VIDEO_UNLOCKED).build());
        }
        try {
            session.getBasicRemote().sendObject(peerMessageRequestVideoStateBuilder.build());
        } catch (Exception e) {
            logger.error(e);
        }
    }

}
