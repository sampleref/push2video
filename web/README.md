# Web GUI served over https

# Build docker image:
1. cd ../web
2. docker build -t push2talk_web --build-arg http_proxy=$http_proxy --build-arg https_proxy=$https_proxy .

# Run docker image:
1. docker run -p 8993:8993 push2talk_web

# View application in browser:
Open https://<host>:8993/