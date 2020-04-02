# GStreamer based push2talk server

Summary:

1. Build uses base docker images at:
    -> dockerhub: nas2docker/gstreamer_grpc:1.0
    -> Built from github Dockerfiles: https://github.com/sampleref/gstreamer


Steps:


1. Be in context root folder, /server

   cd push2video/server

2. Copy Proto files to grpc folder

    cp ../common/protos/*.proto ./protos/

3. Run docker build

    tag=1.0
    docker build -t push2talk_server:${tag} --build-arg http_proxy=$http_proxy --build-arg https_proxy=$https_proxy .

    #To enable test and coverage data, build with arg WITH_TEST=ON as below,
    docker build -t push2talk_server:${tag} --build-arg http_proxy=$http_proxy --build-arg https_proxy=$https_proxy --build-arg no_proxy="127.0.0.1" --build-arg WITH_TEST=ON .

4. Push docker image to repo

    tag=1.0
    docker push push2talk_server:${tag}

5. Running docker container - for test purposes

    docker run -v /mnt/media/av/:/mnt/av --net=host -e GST_DEBUG=3,push2talk_gst:4 push2talk_server:${tag}