FROM openjdk:8-jre

USER root
ADD /push2video-signaller.jar //
ADD /log4j2.xml /conf/

EXPOSE 17003
ENTRYPOINT ["java","-XX:+UnlockExperimentalVMOptions","-XX:+UseCGroupMemoryLimitForHeap","-XX:+PrintFlagsFinal","-jar","-Dlog4j.configurationFile=/conf/log4j2.xml","/push2video-signaller.jar"]