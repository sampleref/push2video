FROM openjdk:8-jre

USER root
ADD target/push2video-signaller.jar //
ADD src/main/resources/log4j2.xml /conf/

EXPOSE 17102
EXPOSE 9443
ENTRYPOINT ["java","-XX:+UnlockExperimentalVMOptions","-XX:+UseCGroupMemoryLimitForHeap","-XX:+PrintFlagsFinal","-jar","-Dlog4j.configurationFile=/conf/log4j2.xml","/push2video-signaller.jar"]