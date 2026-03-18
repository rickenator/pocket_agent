#include "circuit_breaker.h"
#include "esp_log.h"

static const char* TAG = "CIRCUIT_BREAKER";

CircuitBreaker::CircuitBreaker(int failureThreshold, int64_t recoveryTimeUs)
    : m_mutex(nullptr)
    , m_state(CLOSED)
    , m_failureCount(0)
    , m_failureThreshold(failureThreshold > 0 ? failureThreshold : 3)
    , m_recoveryTimeUs(recoveryTimeUs > 0 ? recoveryTimeUs : 30000000LL)
    , m_lastFailureTimeUs(0)
    , m_probeInProgress(false)
{
    m_mutex = xSemaphoreCreateMutex();
}

CircuitBreaker::~CircuitBreaker()
{
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

bool CircuitBreaker::allowRequest()
{
    if (!m_mutex) return true;
    xSemaphoreTake(m_mutex, portMAX_DELAY);

    bool allow = false;
    switch (m_state) {
    case CLOSED:
        allow = true;
        break;

    case OPEN: {
        int64_t now = esp_timer_get_time();
        if ((now - m_lastFailureTimeUs) >= m_recoveryTimeUs) {
            // Transition to HALF_OPEN to probe the service
            m_state = HALF_OPEN;
            ESP_LOGI(TAG, "Circuit HALF_OPEN — probing service");
            allow = true;
        } else {
            ESP_LOGD(TAG, "Circuit OPEN — rejecting request");
            allow = false;
        }
        break;
    }

    case HALF_OPEN:
        // Allow only one probe at a time; block subsequent callers until outcome known
        if (!m_probeInProgress) {
            m_probeInProgress = true;
            allow = true;
        } else {
            ESP_LOGD(TAG, "Circuit HALF_OPEN — probe already in progress");
            allow = false;
        }
        break;
    }

    xSemaphoreGive(m_mutex);
    return allow;
}

void CircuitBreaker::recordSuccess()
{
    if (!m_mutex) return;
    xSemaphoreTake(m_mutex, portMAX_DELAY);

    if (m_state == HALF_OPEN) {
        ESP_LOGI(TAG, "Circuit CLOSED — service recovered");
    }
    m_state           = CLOSED;
    m_failureCount    = 0;
    m_probeInProgress = false;

    xSemaphoreGive(m_mutex);
}

void CircuitBreaker::recordFailure()
{
    if (!m_mutex) return;
    xSemaphoreTake(m_mutex, portMAX_DELAY);

    m_lastFailureTimeUs = esp_timer_get_time();
    ++m_failureCount;
    m_probeInProgress = false;

    if (m_state == HALF_OPEN) {
        // Probe failed — go back to OPEN
        m_state = OPEN;
        ESP_LOGW(TAG, "Circuit OPEN — probe failed");
    } else if (m_state == CLOSED && m_failureCount >= m_failureThreshold) {
        m_state = OPEN;
        ESP_LOGW(TAG, "Circuit OPEN — %d consecutive failures", m_failureCount);
    }

    xSemaphoreGive(m_mutex);
}

CircuitBreaker::State CircuitBreaker::getState() const
{
    return m_state;
}

int CircuitBreaker::getFailureCount() const
{
    return m_failureCount;
}

void CircuitBreaker::reset()
{
    if (!m_mutex) return;
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_state           = CLOSED;
    m_failureCount    = 0;
    m_probeInProgress = false;
    xSemaphoreGive(m_mutex);
}
