Kubernetes Setup Instructions:

1. Replace below variables with actual values in ptt_envoy.yaml   
    <EXTERNAL_IP>           - External IP Address of host machine to be reachable from peers
       
                            - Example: 192.168.10.1    
    <ENVOY_CONFIG_FILE>     - Envoy config file as in ./config/envoy.yaml above
        
                            - Example: /tmp/test/push2video/k8s/config/envoy.yaml    
    <ENVOY_CERT_DIR>        - Envoy certificates directory as in ./certs above    
                          
                            - Example: /tmp/test/push2video/k8s/certs     
2. Run script    
 ptt_install.sh