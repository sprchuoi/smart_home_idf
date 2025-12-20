/**
 * @file EventBus.cpp
 * @brief Event Bus implementation
 */

#include "EventBus.h"
#include <string.h>

const char* EventBus::TAG = "EventBus";

EventBus& EventBus::getInstance() {
    static EventBus instance;
    return instance;
}

bool EventBus::initialize(size_t queue_size) {
    m_queue = xQueueCreate(queue_size, sizeof(EventMessage));
    if (m_queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return false;
    }
    
    m_mutex = xSemaphoreCreateMutex();
    if (m_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
        vQueueDelete(m_queue);
        m_queue = nullptr;
        return false;
    }
    
    m_subscriptions.clear();
    ESP_LOGI(TAG, "EventBus initialized with queue size %zu", queue_size);
    return true;
}

bool EventBus::publish(const EventMessage& event) {
    if (m_queue == nullptr) {
        ESP_LOGE(TAG, "EventBus not initialized");
        return false;
    }
    
    EventMessage msg = event;
    msg.timestamp = xTaskGetTickCount();
    
    BaseType_t result = xQueueSend(m_queue, &msg, pdMS_TO_TICKS(100));
    if (result != pdTRUE) {
        ESP_LOGW(TAG, "Failed to publish event type %d (queue full?)", static_cast<int>(event.type));
        return false;
    }
    
    return true;
}

size_t EventBus::subscribe(EventType event_type, EventCallback callback) {
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex for subscription");
        return SIZE_MAX;
    }
    
    Subscription sub;
    sub.type = event_type;
    sub.callback = callback;
    m_subscriptions.push_back(sub);
    
    size_t id = m_subscriptions.size() - 1;
    
    xSemaphoreGive(m_mutex);
    ESP_LOGI(TAG, "Subscribed to event type %d (ID: %zu)", static_cast<int>(event_type), id);
    return id;
}

void EventBus::processEvents() {
    if (m_queue == nullptr) {
        ESP_LOGE(TAG, "EventBus not initialized");
        return;
    }
    
    EventMessage event;
    if (xQueueReceive(m_queue, &event, portMAX_DELAY) == pdTRUE) {
        // Notify all subscribers
        if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            for (const auto& sub : m_subscriptions) {
                if (sub.type == event.type || sub.type == EventType::SYSTEM_ERROR) {
                    try {
                        sub.callback(event);
                    } catch (...) {
                        ESP_LOGE(TAG, "Exception in event callback");
                    }
                }
            }
            xSemaphoreGive(m_mutex);
        }
    }
}

void EventBus::deinitialize() {
    if (m_queue != nullptr) {
        vQueueDelete(m_queue);
        m_queue = nullptr;
    }
    
    if (m_mutex != nullptr) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
    
    m_subscriptions.clear();
    ESP_LOGI(TAG, "EventBus deinitialized");
}

