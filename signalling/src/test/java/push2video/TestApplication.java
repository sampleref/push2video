package push2video;

import org.apache.logging.log4j.LogManager;
import push2video.grpc.Push2VideoGrpcClientImpl;
import push2video.utils.Constants;
import push2video.utils.Push2VideoUtils;

class TestApplication {
    private static final org.apache.logging.log4j.Logger logger = LogManager.getLogger();

    public static void main(String[] args) throws Exception {
        createTestClientCheckPing();
    }

    public static void createTestClientCheckPing() throws Exception {
        Push2VideoGrpcClientImpl push2VideoGrpcClient = Push2VideoGrpcClientImpl.getInstance("127.0.0.1", Constants.GRPC_CLIENT_PORT);
        push2VideoGrpcClient.checkPing();
        logger.info("Sent Ping Message");
    }

}