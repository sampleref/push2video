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
const int GRPC_SERVER_PORT = 17101;
const int GRPC_SIGNALLING_PORT = 17102;

class AudioPipelineHandler;
typedef std::shared_ptr<AudioPipelineHandler> AudioPipelineHandlerPtr;

class PushToTalkServiceClient;
typedef std::shared_ptr<PushToTalkServiceClient> PushToTalkServiceClientPtr;

namespace push2talkUtils {
    extern std::map<std::string, AudioPipelineHandlerPtr> audioPipelineHandlers;
    extern PushToTalkServiceClientPtr pushToTalkServiceClientPtr;
    extern std::mutex peers_mutex;

    int generate_random_int(void);

    int initialise_rand(void);

    void add_audiopipelinehandler_to_map(std::string meetingId, AudioPipelineHandlerPtr audioPipelineHandlerPtr);

    AudioPipelineHandlerPtr fetch_audio_pipelinehandler_by_key(std::string key);

}

#endif //PUSHTOTALKSERVICE_PUSH2TALKUTILS_HPP
