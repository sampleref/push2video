FROM nas2docker/gstreamer_grpc:1.2

ADD . /appsrc/

# For Release build replace 'Debug' with 'Release' in below RUN
RUN cd /appsrc/ \
    && mkdir -p Debug \
    && cd Debug \
    && cmake -DCMAKE_BUILD_TYPE=Debug .. \
    && make

CMD ["/appsrc/Debug/push2talk_server"]