/**
 * @file AudioStateMachine.cpp
 * @brief Audio State Machine implementation
 */

#include "AudioStateMachine.h"
#include "error/ErrorHandler.h"

const char* AudioStateMachine::TAG = "AudioStateMachine";

AudioStateMachine::AudioStateMachine()
    : m_task_handle(nullptr)
    , m_current_state(AudioState::AUDIO_INIT)
    , m_initialized(false) {
}

AudioStateMachine::~AudioStateMachine() {
    stop();
}

bool AudioStateMachine::initialize() {
    if (m_initialized) {
        return true;
    }
    
    // Subscribe to relevant events
    EventBus::getInstance().subscribe(EventType::WAKE_WORD_DETECTED, [this](const EventMessage& event) {
        handleEvent(event);
    });
    
    EventBus::getInstance().subscribe(EventType::STATE_CHANGED, [this](const EventMessage& event) {
        handleEvent(event);
    });
    
    EventBus::getInstance().subscribe(EventType::SYSTEM_ERROR, [this](const EventMessage& event) {
        handleEvent(event);
    });
    
    // Create task on Core 1
    BaseType_t result = xTaskCreatePinnedToCore(
        taskEntry,
        "AudioStateMachine",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &m_task_handle,
        TASK_CORE
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio state machine task");
        return false;
    }
    
    m_initialized = true;
    transitionTo(AudioState::AUDIO_INIT);
    ESP_LOGI(TAG, "AudioStateMachine initialized (Core %d)", TASK_CORE);
    return true;
}

void AudioStateMachine::stop() {
    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }
    
    m_initialized = false;
}

std::string AudioStateMachine::getStateString() const {
    switch (m_current_state) {
        case AudioState::AUDIO_INIT: return "AUDIO_INIT";
        case AudioState::AUDIO_IDLE: return "AUDIO_IDLE";
        case AudioState::AUDIO_LISTENING: return "AUDIO_LISTENING";
        case AudioState::AUDIO_WAKE_DETECTED: return "AUDIO_WAKE_DETECTED";
        case AudioState::AUDIO_PROCESSING: return "AUDIO_PROCESSING";
        case AudioState::AUDIO_SUSPENDED: return "AUDIO_SUSPENDED";
        default: return "UNKNOWN";
    }
}

bool AudioStateMachine::requestTransition(AudioState new_state) {
    if (!isValidTransition(m_current_state, new_state)) {
        ESP_LOGW(TAG, "Invalid transition: %s -> %s",
                getStateString().c_str(),
                [new_state]() {
                    switch (new_state) {
                        case AudioState::AUDIO_INIT: return "AUDIO_INIT";
                        case AudioState::AUDIO_IDLE: return "AUDIO_IDLE";
                        case AudioState::AUDIO_LISTENING: return "AUDIO_LISTENING";
                        case AudioState::AUDIO_WAKE_DETECTED: return "AUDIO_WAKE_DETECTED";
                        case AudioState::AUDIO_PROCESSING: return "AUDIO_PROCESSING";
                        case AudioState::AUDIO_SUSPENDED: return "AUDIO_SUSPENDED";
                        default: return "UNKNOWN";
                    }
                }());
        return false;
    }
    
    transitionTo(new_state);
    return true;
}

void AudioStateMachine::taskLoop() {
    ESP_LOGI(TAG, "Audio state machine task started on Core %d", xPortGetCoreID());
    
    // Process events from EventBus
    while (true) {
        EventBus::getInstance().processEvents();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void AudioStateMachine::handleEvent(const EventMessage& event) {
    switch (m_current_state) {
        case AudioState::AUDIO_INIT:
            if (event.type == EventType::STATE_CHANGED) {
                // Check if app is in RUNNING state
                requestTransition(AudioState::AUDIO_IDLE);
            }
            break;
            
        case AudioState::AUDIO_IDLE:
            if (event.type == EventType::STATE_CHANGED) {
                // App state changed, may need to start listening
                requestTransition(AudioState::AUDIO_LISTENING);
            }
            break;
            
        case AudioState::AUDIO_LISTENING:
            if (event.type == EventType::WAKE_WORD_DETECTED) {
                requestTransition(AudioState::AUDIO_WAKE_DETECTED);
            } else if (event.type == EventType::SYSTEM_ERROR) {
                requestTransition(AudioState::AUDIO_SUSPENDED);
            }
            break;
            
        case AudioState::AUDIO_WAKE_DETECTED:
            // Transition to processing or back to listening
            requestTransition(AudioState::AUDIO_PROCESSING);
            break;
            
        case AudioState::AUDIO_PROCESSING:
            // After processing, return to listening
            requestTransition(AudioState::AUDIO_LISTENING);
            break;
            
        case AudioState::AUDIO_SUSPENDED:
            // Can resume on app state change
            if (event.type == EventType::STATE_CHANGED) {
                requestTransition(AudioState::AUDIO_IDLE);
            }
            break;
    }
}

void AudioStateMachine::transitionTo(AudioState new_state) {
    if (new_state == m_current_state) {
        return;
    }
    
    ESP_LOGI(TAG, "Audio state transition: %s -> %s",
            getStateString().c_str(),
            [new_state]() {
                switch (new_state) {
                    case AudioState::AUDIO_INIT: return "AUDIO_INIT";
                    case AudioState::AUDIO_IDLE: return "AUDIO_IDLE";
                    case AudioState::AUDIO_LISTENING: return "AUDIO_LISTENING";
                    case AudioState::AUDIO_WAKE_DETECTED: return "AUDIO_WAKE_DETECTED";
                    case AudioState::AUDIO_PROCESSING: return "AUDIO_PROCESSING";
                    case AudioState::AUDIO_SUSPENDED: return "AUDIO_SUSPENDED";
                    default: return "UNKNOWN";
                }
            }());
    
    onStateExit(m_current_state);
    m_current_state = new_state;
    onStateEnter(m_current_state);
    
    // Publish audio state change event
    EventMessage event;
    event.type = EventType::STATE_CHANGED;
    event.source = EventSource::STATE_MACHINE;
    event.destination = EventSource::APPLICATION;
    event.payload.state_info.state_name = getStateString().c_str();
    
    EventBus::getInstance().publish(event);
}

bool AudioStateMachine::isValidTransition(AudioState from, AudioState to) {
    // Define valid transitions
    switch (from) {
        case AudioState::AUDIO_INIT:
            return to == AudioState::AUDIO_IDLE || to == AudioState::AUDIO_SUSPENDED;
            
        case AudioState::AUDIO_IDLE:
            return to == AudioState::AUDIO_LISTENING || to == AudioState::AUDIO_SUSPENDED;
            
        case AudioState::AUDIO_LISTENING:
            return to == AudioState::AUDIO_WAKE_DETECTED || 
                   to == AudioState::AUDIO_IDLE || 
                   to == AudioState::AUDIO_SUSPENDED;
            
        case AudioState::AUDIO_WAKE_DETECTED:
            return to == AudioState::AUDIO_PROCESSING || to == AudioState::AUDIO_LISTENING;
            
        case AudioState::AUDIO_PROCESSING:
            return to == AudioState::AUDIO_LISTENING || to == AudioState::AUDIO_IDLE;
            
        case AudioState::AUDIO_SUSPENDED:
            return to == AudioState::AUDIO_IDLE || to == AudioState::AUDIO_INIT;
            
        default:
            return false;
    }
}

void AudioStateMachine::onStateEnter(AudioState state) {
    switch (state) {
        case AudioState::AUDIO_INIT:
            ESP_LOGI(TAG, "Entering AUDIO_INIT state");
            break;
            
        case AudioState::AUDIO_IDLE:
            ESP_LOGI(TAG, "Entering AUDIO_IDLE state");
            break;
            
        case AudioState::AUDIO_LISTENING:
            ESP_LOGI(TAG, "Entering AUDIO_LISTENING state");
            break;
            
        case AudioState::AUDIO_WAKE_DETECTED:
            ESP_LOGI(TAG, "Entering AUDIO_WAKE_DETECTED state");
            break;
            
        case AudioState::AUDIO_PROCESSING:
            ESP_LOGI(TAG, "Entering AUDIO_PROCESSING state");
            break;
            
        case AudioState::AUDIO_SUSPENDED:
            ESP_LOGI(TAG, "Entering AUDIO_SUSPENDED state");
            break;
    }
}

void AudioStateMachine::onStateExit(AudioState state) {
    (void)state;  // Suppress unused parameter warning
}

void AudioStateMachine::taskEntry(void* parameter) {
    AudioStateMachine* fsm = static_cast<AudioStateMachine*>(parameter);
    fsm->taskLoop();
}

