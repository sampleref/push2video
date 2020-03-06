package push2video.websocket;

import com.google.protobuf.util.JsonFormat;
import com.push2talk.rpc.events.PeerMessageRequest;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import javax.websocket.Decoder;
import javax.websocket.EndpointConfig;

/**
 * @author chakra
 *
 */
public class MessageDecoder implements Decoder.Text<PeerMessageRequest> {

	private static final Logger logger = LogManager.getLogger(MessageDecoder.class);
	private static JsonFormat.Parser praser = JsonFormat.parser();

	@Override
	public void destroy() {
		logger.info("MessageDecoder - destroy method called");
	}

	@Override
	public void init(EndpointConfig endpointConfig) {
		logger.info("MessageDecoder - init method called");
	}

	@Override
	public PeerMessageRequest decode(String message) {
		PeerMessageRequest.Builder peerMessageBuilder = PeerMessageRequest.newBuilder();
		try {
			praser.merge(message, peerMessageBuilder);
			return peerMessageBuilder.build();
		} catch (Exception ex) {
			logger.error("Exception while decoding message " + ex.getMessage());
			return null;
		}

	}

	@Override
	public boolean willDecode(String message) {
		return true;
	}

}
