#!/bin/bash
kubectl create namespace ptt-setup

kubectl apply -f ptt_server.yaml -n ptt-setup
kubectl apply -f ptt_signalling.yaml -n ptt-setup
kubectl apply -f ptt_web.yaml -n ptt-setup
kubectl apply -f ptt_envoy.yaml -n ptt-setup
