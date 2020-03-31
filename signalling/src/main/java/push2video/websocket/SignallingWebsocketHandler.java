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
        if (GrpcWebsocketBridge.updateLockAndVerify(message)) {
            GrpcWebsocketBridge.sendToServer(GrpcWebsocketBridge.sessionPeerMap.get(session.getId()), message);
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
        PeerMessageRequest peerMessageRequestVideoState;
        if (GrpcWebsocketBridge.videoLock()) {
            peerMessageRequestVideoState = PeerMessageRequest.newBuilder().setPeerId(randomString)
                    .setChannelStatusMessage(ChannelStatusMessage.newBuilder().setStatus(ChannelStatusMessage.Status.VIDEO_LOCKED).build())
                    .build();
        } else {
            peerMessageRequestVideoState = PeerMessageRequest.newBuilder().setPeerId(randomString)
                    .setChannelStatusMessage(ChannelStatusMessage.newBuilder().setStatus(ChannelStatusMessage.Status.VIDEO_UNLOCKED).build())
                    .build();
        }
        try {
            session.getBasicRemote().sendObject(peerMessageRequestConnected);
            session.getBasicRemote().sendObject(peerMessageRequestVideoState);
        } catch (Exception e) {
            logger.error(e);
            return "";
        }
        return randomString;
    }

}
