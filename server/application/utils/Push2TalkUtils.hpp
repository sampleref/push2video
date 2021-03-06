//
// Created by chakra on 03-03-2020.
//

#ifndef PUSHTOTALKSERVICE_PUSH2TALKUTILS_HPP
#define PUSHTOTALKSERVICE_PUSH2TALKUTILS_HPP

#include <mutex>
#include <memory>
#include <map>

/*
 * Constants
 */
const int GRPC_SERVER_PORT = 18101;
const int GRPC_SIGNALLING_PORT = 18102;

const bool APPLY_WATCHDOG_TIMEOUT = false;

class AudioPipelineHandler;
typedef std::shared_ptr<AudioPipelineHandler> AudioPipelineHandlerPtr;

class VideoPipelineHandler;
typedef std::shared_ptr<VideoPipelineHandler> VideoPipelineHandlerPtr;

class GroupSendRecvHandler;
typedef std::shared_ptr<GroupSendRecvHandler> GroupSendRecvHandlerPtr;

class PushToTalkServiceClient;
typedef std::shared_ptr<PushToTalkServiceClient> PushToTalkServiceClientPtr;

namespace push2talkUtils {
    extern std::map<std::string, AudioPipelineHandlerPtr> audioPipelineHandlers;
    extern std::map<std::string, VideoPipelineHandlerPtr> videoPipelineHandlers;
    extern std::map<std::string, GroupSendRecvHandlerPtr> groupSendRecvHandlers;
    extern PushToTalkServiceClientPtr pushToTalkServiceClientPtr;
    extern std::mutex peers_mutex;
    extern std::mutex ue_sdp_mutex;

    int generate_random_int(void);

    int initialise_rand(void);

    void add_audiopipelinehandler_to_map(std::string channelId, AudioPipelineHandlerPtr audioPipelineHandlerPtr);

    AudioPipelineHandlerPtr fetch_audio_pipelinehandler_by_key(std::string key);

    void add_videopipelinehandler_to_map(std::string channelId, VideoPipelineHandlerPtr videoPipelineHandlerPtr);

    VideoPipelineHandlerPtr fetch_video_pipelinehandler_by_key(std::string key);

    void remove_video_pipeline_handler(std::string channelId);

    void remove_audio_pipeline_handler(std::string channelId);

    void add_groupsendrecvhandler_to_map(std::string groupId, GroupSendRecvHandlerPtr groupSendRecvHandlerPtr);

    GroupSendRecvHandlerPtr fetch_groupsendrecvhandler_by_groupid(std::string key);

    void remove_groupsendrecv_handler(std::string groupId);

}

#endif //PUSHTOTALKSERVICE_PUSH2TALKUTILS_HPP
