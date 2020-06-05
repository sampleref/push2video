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
    std::map<std::string, VideoPipelineHandlerPtr> videoPipelineHandlers = {};
    std::map<std::string, GroupSendRecvHandlerPtr> groupSendRecvHandlers = {};
    PushToTalkServiceClientPtr pushToTalkServiceClientPtr;
    std::mutex lock_mutex;
    std::mutex peers_mutex;
    std::mutex ue_sdp_mutex;

    int generate_random_int(void) {
        return rand();
    }

    int initialise_rand(void) {
        srand(time(0));
    }

    void add_audiopipelinehandler_to_map(std::string channelId, AudioPipelineHandlerPtr audioPipelineHandlerPtr) {
        lock_mutex.lock();
        audioPipelineHandlers[channelId] = audioPipelineHandlerPtr;
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

    void add_videopipelinehandler_to_map(std::string channelId, VideoPipelineHandlerPtr videoPipelineHandlerPtr) {
        lock_mutex.lock();
        videoPipelineHandlers[channelId] = videoPipelineHandlerPtr;
        lock_mutex.unlock();
    }

    VideoPipelineHandlerPtr fetch_video_pipelinehandler_by_key(std::string key) {
        lock_mutex.lock();
        auto it_pipeline = videoPipelineHandlers.find(key);
        if (it_pipeline != videoPipelineHandlers.end()) {
            lock_mutex.unlock();
            return it_pipeline->second;
        } else {
            lock_mutex.unlock();
            return NULL;
        }
    }

    void remove_video_pipeline_handler(std::string channelId) {
        std::lock_guard<std::mutex> lockGuard(push2talkUtils::lock_mutex);
        auto it_peer = videoPipelineHandlers.find(channelId);
        if (it_peer != videoPipelineHandlers.end()) {
            videoPipelineHandlers.erase(it_peer);
            GST_INFO("Deleted video pipeline handler from map for channel %s", channelId.c_str());
        }
    }

    void remove_audio_pipeline_handler(std::string channelId) {
        std::lock_guard<std::mutex> lockGuard(push2talkUtils::lock_mutex);
        auto it_peer = audioPipelineHandlers.find(channelId);
        if (it_peer != audioPipelineHandlers.end()) {
            audioPipelineHandlers.erase(it_peer);
            GST_INFO("Deleted audio pipeline handler from map for channel %s", channelId.c_str());
        }
    }

    void add_groupsendrecvhandler_to_map(std::string groupId, GroupSendRecvHandlerPtr groupSendRecvHandlerPtr) {
        lock_mutex.lock();
        groupSendRecvHandlers[groupId] = groupSendRecvHandlerPtr;
        lock_mutex.unlock();
    }

    GroupSendRecvHandlerPtr fetch_groupsendrecvhandler_by_groupid(std::string key) {
        lock_mutex.lock();
        auto it_pipeline = groupSendRecvHandlers.find(key);
        if (it_pipeline != groupSendRecvHandlers.end()) {
            lock_mutex.unlock();
            return it_pipeline->second;
        } else {
            lock_mutex.unlock();
            return NULL;
        }
    }

    void remove_groupsendrecv_handler(std::string groupId) {
        std::lock_guard<std::mutex> lockGuard(push2talkUtils::lock_mutex);
        auto it_peer = groupSendRecvHandlers.find(groupId);
        if (it_peer != groupSendRecvHandlers.end()) {
            groupSendRecvHandlers.erase(it_peer);
            GST_INFO("Deleted send recv handler from map for group %s", groupId.c_str());
        }
    }
}