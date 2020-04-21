package push2video;

import org.apache.logging.log4j.LogManager;
import push2video.bridge.GrpcWebsocketBridge;
import push2video.grpc.Push2VideoClient;
import push2video.grpc.Push2VideoService;
import push2video.utils.Constants;
import push2video.utils.Push2VideoUtils;
import push2video.websocket.WebSocketServer;

public class Application {
    private static final org.apache.logging.log4j.Logger logger = LogManager.getLogger();
    private static Push2VideoService service;
    private static WebSocketServer websocketServer;
    private static Push2VideoClient push2VideoClient;

    public static void main(String[] args) {
        try {
            websocketServer = new WebSocketServer();
            websocketServer.initialize(false);
            websocketServer.start();
            push2VideoClient = new Push2VideoClient();
            GrpcWebsocketBridge.initialize(push2VideoClient);
            service = new Push2VideoService(Constants.GRPC_CLIENT_PORT);
            service.start();
            //Shutdown hook
            Runtime.getRuntime().addShutdownHook(new Thread() {
                @Override
                public void run() {
                    kill();
                }
            });
            push2VideoClient.updateSignallingStart(Push2VideoUtils.getEnv(Constants.CLIENT_HOST_KEY, Constants.CLIENT_HOST_DEF), Constants.GRPC_CLIENT_PORT);
            push2VideoClient.createAudioChannel(Constants.DEFUALT_AUDIO_CHANNEL);
            service.waitUntilCompletion();
        } catch (Exception e) {
            logger.error("While starting service, error message {} ", e.getMessage());
            System.exit(1);
        }
    }

    public static void kill() {
        logger.error("Stopping the service. JVM is shutting down");
        service.stop();
    }
}