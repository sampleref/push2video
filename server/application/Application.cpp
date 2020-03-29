//
// Created by Chakra on 27-02-2020.
//

#include <gst/gst.h>
#include <string.h>
#include <stdio.h>
#include <regex>
#include <stdlib.h>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

#include "grpc/GrpcService.hpp"
#include "utils/Push2TalkUtils.hpp"

using namespace std;

GST_DEBUG_CATEGORY_EXTERN (push2talk_gst);
#define GST_CAT_DEFAULT push2talk_gst

std::shared_ptr<GrpcServer> grpcServer;

void SIGSEGV_handler(int sig) {
    void *array[10];
    size_t size;
    // get void*'s for all entries on the stack
    size = backtrace(array, 10);
    GST_ERROR("SIGSEGV_handler Error: signal %d: ", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    signal(sig, SIG_DFL);
    kill(getpid(), sig);
    _Exit(0);
}

void Stop_Signal_handler(int sig) {
    GST_ERROR("Stop_Signal_handler Error: signal %d: ", sig);

    signal(sig, SIG_DFL);
    kill(getpid(), sig);
    _Exit(0);
}

int
main(int argc, char *argv[]) {

    signal(SIGSEGV, SIGSEGV_handler);
    signal(SIGABRT, Stop_Signal_handler);
    signal(SIGTERM, Stop_Signal_handler);

    push2talkUtils::initialise_rand();

    gst_init(&argc, &argv);
    GST_DEBUG_CATEGORY_INIT (push2talk_gst, "push2talk_gst", 2, "Custom GStreamer Push2Talk Logging Category");

    grpcServer = std::make_shared<GrpcServer>();
    grpcServer->startServer();
}