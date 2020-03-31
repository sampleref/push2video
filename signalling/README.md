# WebRTC peers information and signalling mechanism 

# Build docker image:
1. cd ../signalling
2. mvn clean install 
Or 
1. For docker image: docker build -t push2talk_signalling .

# Run docker image:
1. docker run -p 9443:9443 -p 17102:17102 push2talk_web