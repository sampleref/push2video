/**
 *
 */
package push2video.websocket;

import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;
import org.eclipse.jetty.http.HttpVersion;
import org.eclipse.jetty.server.*;
import org.eclipse.jetty.servlet.FilterHolder;
import org.eclipse.jetty.servlet.ServletContextHandler;
import org.eclipse.jetty.servlets.CrossOriginFilter;
import org.eclipse.jetty.util.resource.Resource;
import org.eclipse.jetty.util.ssl.SslContextFactory;
import org.eclipse.jetty.websocket.jsr356.server.deploy.WebSocketServerContainerInitializer;
import push2video.utils.Constants;

import javax.servlet.DispatcherType;
import javax.websocket.server.ServerContainer;
import java.util.EnumSet;

/**
 * @author chakra
 */

public class WebSocketServer {

    private static final Logger logger = LogManager.getLogger(WebSocketServer.class);
    private Server server;
    private String host = "0.0.0.0"; //Default host
    private Integer port = 9443; // Default port
    private Resource keyStoreResource;
    private String keyStorePassword;
    private String keyManagerPassword;

    /**
     * @param enableSSl
     * @throws Exception
     */
    private void configure(boolean enableSSl) throws Exception {
        logger.info("Initializing the websocket server");
        ServerConnector serverConnector = null;
        server = new Server();

        if (enableSSl) {
            serverConnector = getSslConnector();
        } else {
            serverConnector = new ServerConnector(server);
            serverConnector.setHost(host);
            serverConnector.setPort(port);
        }
        server.addConnector(serverConnector);
        ServletContextHandler context = new ServletContextHandler(ServletContextHandler.SESSIONS);
        context.setServer(server);
        FilterHolder holder = new FilterHolder(CrossOriginFilter.class);
        holder.setInitParameter(CrossOriginFilter.ALLOWED_ORIGINS_PARAM, "*");
        holder.setInitParameter(CrossOriginFilter.ACCESS_CONTROL_ALLOW_ORIGIN_HEADER, "*");
        holder.setInitParameter(CrossOriginFilter.ALLOWED_METHODS_PARAM, "GET,POST,HEAD");
        holder.setInitParameter(CrossOriginFilter.ALLOWED_HEADERS_PARAM, "X-Requested-With,Content-Type,Accept,Origin");
        holder.setName("cross-origin");
        context.addFilter(holder, "*", EnumSet.of(DispatcherType.REQUEST));
        ServerContainer wsContainer = WebSocketServerContainerInitializer.configureContext(context);
        wsContainer.addEndpoint(SignallingWebsocketHandler.class);
        server.setHandler(context);
    }

    public void initialize(boolean enableSsl) throws Exception {
        setHost(Constants.HOST_ADDR);
        setPort(port);
        setKeyStoreResource(Resource.newResource(this.getClass().getClassLoader().getResource(Constants.JKSFILE)));
        setKeyStorePassword(Constants.CREDENTIAL);
        setKeyManagerPassword(Constants.CREDENTIAL);
        configure(enableSsl);
    }

    /**
     * @return
     */
    public ServerConnector getSslConnector() {
        SslContextFactory sslContextFactory = new SslContextFactory();
        sslContextFactory.setKeyStoreResource(keyStoreResource);
        sslContextFactory.setKeyStorePassword(keyStorePassword);
        sslContextFactory.setKeyManagerPassword(keyManagerPassword);
        SslConnectionFactory sslConnectionFactory = new SslConnectionFactory(sslContextFactory,
                HttpVersion.HTTP_1_1.asString());
        HttpConnectionFactory httpConnectionFactory = new HttpConnectionFactory(new HttpConfiguration());
        ServerConnector sslConnector = new ServerConnector(server, sslConnectionFactory, httpConnectionFactory);
        sslConnector.setHost(host);
        sslConnector.setPort(port);
        return sslConnector;
    }

    /**
     * @throws Exception
     */
    public void start() throws Exception {
        logger.info("Starting websocket server");
        server.start();
        logger.info("Websocket server started");
    }

    /**
     * @throws Exception
     */
    public void stop() throws Exception {
        logger.info("Stopping websocket server");
        server.stop();
        logger.info("Stopped websocket server");
    }

    /**
     * @param host
     */
    public void setHost(String host) {
        this.host = host;
    }

    /**
     * @param port
     */
    public void setPort(int port) {
        this.port = port;
    }

    /**
     * @param keyStoreResource
     */
    public void setKeyStoreResource(Resource keyStoreResource) {
        this.keyStoreResource = keyStoreResource;
    }

    /**
     * @param keyStorePassword
     */
    public void setKeyStorePassword(String keyStorePassword) {
        this.keyStorePassword = keyStorePassword;
    }

    /**
     * @param keyManagerPassword
     */
    public void setKeyManagerPassword(String keyManagerPassword) {
        this.keyManagerPassword = keyManagerPassword;
    }

}
