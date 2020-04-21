# WebRTC peers information and signalling mechanism 

# Build docker image:
1. cd ../signalling
2. mvn clean install 
3. docker build -t push2talk_signalling:1.0 --build-arg http_proxy=$http_proxy --build-arg https_proxy=$https_proxy .

# Run docker image:
1. docker run -p 9443:9443 -p 18102:18102 -e GST_SERVER_HOST=127.0.0.1 -e SIGNALLING_CLIENT_HOST=127.0.0.1 push2talk_signalling:1.0