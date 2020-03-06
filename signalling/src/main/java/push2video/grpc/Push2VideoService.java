package push2video.grpc;

import io.grpc.Server;
import io.grpc.ServerBuilder;
import org.apache.logging.log4j.LogManager;

import java.io.IOException;

public class Push2VideoService {

    private static final org.apache.logging.log4j.Logger logger = LogManager.getLogger();
    private Push2VideoGrpcServiceImpl push2VideoGrpcService;
    private Server server;

    public Push2VideoService(int port){
        logger.info("Initializing the grpc service...");
        try {
            push2VideoGrpcService = new Push2VideoGrpcServiceImpl();
            ServerBuilder serverBuilder = ServerBuilder.forPort(port).addService(push2VideoGrpcService);
            server = serverBuilder.build();
        } catch (Exception e) {
            logger.error("While initializing the service " + e.getMessage());
            logger.info(e);
            throw e;
        }
    }

    public void start() throws IOException {
        server.start();
        logger.info("Started Grpc service");
    }

    public void waitUntilCompletion() throws InterruptedException {
        if (server != null)
            server.awaitTermination();
    }

    public void stop() {
        server.shutdown();
    }
}
