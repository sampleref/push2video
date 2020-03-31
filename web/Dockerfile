FROM nginx:alpine

USER root

ADD index.html /appsrc/
ADD push2video.js /appsrc/
ADD key.pem /appsrc/
ADD cert.pem /appsrc/

ADD nginx.conf /etc/nginx/

