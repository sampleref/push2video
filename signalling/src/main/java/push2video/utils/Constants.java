package push2video.utils;

public class Constants {

    public static final int GRPC_SERVER_PORT = 17101;
    public static final int GRPC_CLIENT_PORT = 17102;

    public static final Long IDLE_WEBSOCKET_TIMEOUT = 30 * 60000L;

    // SSL
    public static final String CREDENTIAL = "password";
    public static final String JKSFILE = "server.jks";
    public static final String HOST_ADDR = "0.0.0.0";

    //Default Audio Meeting Id
    public static final String DEFUALT_AUDIO_MEETING = "AUDI0_001_TEKSI";
}
