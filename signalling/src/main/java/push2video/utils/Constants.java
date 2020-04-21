package push2video.utils;

public class Constants {

    public static final int GRPC_SERVER_PORT = 18101;
    public static final int GRPC_CLIENT_PORT = 18102;

    public static final Long IDLE_WEBSOCKET_TIMEOUT = 30 * 60000L;

    // SSL
    public static final String CREDENTIAL = "password";
    public static final String JKSFILE = "server.jks";
    public static final String HOST_ADDR = "0.0.0.0";

    // Hosts
    public static final String SERVER_HOST_KEY = "GST_SERVER_HOST";
    public static final String SERVER_HOST_DEF = "127.0.0.1";
    public static final String CLIENT_HOST_KEY = "SIGNALLING_CLIENT_HOST";
    public static final String CLIENT_HOST_DEF = "127.0.0.1";

    //Default Audio Channel Id
    public static final String DEFUALT_AUDIO_CHANNEL = "AUDI0_001_CHANNEL";
}
