//
// Created by chakra on 03-03-2020.
//
#include <gst/gst.h>
#include "Push2TalkUtils.hpp"
#include "../grpc/GrpcService.hpp"

GST_DEBUG_CATEGORY (push2talk_gst);
#define GST_CAT_DEFAULT push2talk_gst


namespace push2talkUtils {
    std::map<std::string, AudioPipelineHandlerPtr> audioPipelineHandlers = {};
    PushToTalkServiceClientPtr pushToTalkServiceClientPtr;
    std::mutex lock_mutex;
    std::mutex peers_mutex;

    int generate_random_int(void) {
        return rand();
    }

    int initialise_rand(void) {
        srand(time(0));
    }

    void add_audiopipelinehandler_to_map(std::string meetingId, AudioPipelineHandlerPtr audioPipelineHandlerPtr) {
        lock_mutex.lock();
        audioPipelineHandlers[meetingId] = audioPipelineHandlerPtr;
        lock_mutex.unlock();
    }

    AudioPipelineHandlerPtr fetch_audio_pipelinehandler_by_key(std::string key) {
        lock_mutex.lock();
        auto it_pipeline = audioPipelineHandlers.find(key);
        if (it_pipeline != audioPipelineHandlers.end()) {
            lock_mutex.unlock();
            return it_pipeline->second;
        } else {
            lock_mutex.unlock();
            return NULL;
        }
    }
}