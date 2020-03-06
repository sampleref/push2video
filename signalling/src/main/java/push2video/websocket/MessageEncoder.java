package push2video.websocket;

import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.push2talk.rpc.events.PeerMessageRequest;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import javax.websocket.EncodeException;
import javax.websocket.Encoder;
import javax.websocket.EndpointConfig;

/**
 * @author chakra
 *
 */
public class MessageEncoder implements Encoder.Text<PeerMessageRequest> {

	private static final Logger logger = LogManager.getLogger(MessageEncoder.class);

	private static JsonFormat.Printer printer = JsonFormat.printer();

	@Override
	public void destroy() {
		logger.info("MessageEncoder - destroy method called");
	}

	@Override
	public void init(EndpointConfig endpointConfig) {
		logger.info("MessageEncoder - init method called");
	}

	@Override
	public String encode(PeerMessageRequest peerMessageRequest) throws EncodeException {
		try {
			return printer.print(peerMessageRequest);
		} catch (InvalidProtocolBufferException e) {
			logger.error("Exceptions in encoding message " + e.getMessage());
			return null;
		}
	}

}
