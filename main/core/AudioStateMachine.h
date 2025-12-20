/**
 * @file AudioStateMachine.h
 * @brief Audio State Machine (Core 1)
 * 
 * Manages audio pipeline state transitions.
 * Coordinates with AppStateMachine and PowerManager.
 */

#pragma once

#include "core/EventBus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string>

/**
 * @brief Audio states
 */
enum class AudioState : uint8_t {
    AUDIO_INIT,
    AUDIO_IDLE,
    AUDIO_LISTENING,
    AUDIO_WAKE_DETECTED,
    AUDIO_PROCESSING,
    AUDIO_SUSPENDED
};

/**
 * @brief Audio State Machine
 * 
 * Manages audio pipeline state transitions
 */
class AudioStateMachine {
public:
    AudioStateMachine();
    ~AudioStateMachine();
    
    /**
     * @brief Initialize audio state machine
     * @return true on success
     */
    bool initialize();
    
    /**
     * @brief Get current state
     * @return Current state
     */
    AudioState getState() const { return m_current_state; }
    
    /**
     * @brief Get current state as string
     * @return State name
     */
    std::string getStateString() const;
    
    /**
     * @brief Request state transition
     * @param new_state Target state
     * @return true if transition is valid
     */
    bool requestTransition(AudioState new_state);
    
    /**
     * @brief Stop state machine
     */
    void stop();
    
    /**
     * @brief FreeRTOS task entry point
     */
    static void taskEntry(void* parameter);

private:
    void taskLoop();
    void handleEvent(const EventMessage& event);
    void transitionTo(AudioState new_state);
    void onStateEnter(AudioState state);
    void onStateExit(AudioState state);
    bool isValidTransition(AudioState from, AudioState to);
    
    TaskHandle_t m_task_handle;
    AudioState m_current_state;
    bool m_initialized;
    
    static const char* TAG;
    static constexpr int TASK_STACK_SIZE = 4096;
    static constexpr int TASK_PRIORITY = 4;
    static constexpr BaseType_t TASK_CORE = 1;  // Core 1
};

