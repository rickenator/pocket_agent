#ifndef CIRCUIT_BREAKER_H
#define CIRCUIT_BREAKER_H

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdint.h>

// Circuit breaker pattern for network service resilience.
//
// States:
//   CLOSED    — requests pass through normally
//   OPEN      — requests are rejected immediately (service assumed down)
//   HALF_OPEN — one probe request allowed; success → CLOSED, failure → OPEN
//
// Usage:
//   CircuitBreaker cb;
//   if (cb.allowRequest()) {
//       esp_err_t err = doNetworkCall();
//       if (err == ESP_OK) cb.recordSuccess();
//       else               cb.recordFailure();
//   } else {
//       // skip call, return cached/fallback response
//   }
class CircuitBreaker {
public:
    enum State { CLOSED, OPEN, HALF_OPEN };

    // failureThreshold — consecutive failures before opening the circuit
    // recoveryTimeUs   — microseconds in OPEN state before trying HALF_OPEN
    explicit CircuitBreaker(int failureThreshold = 3,
                            int64_t recoveryTimeUs = 30000000LL /* 30 s */);
    ~CircuitBreaker();

    // Returns true if the caller should proceed with the network request.
    bool allowRequest();

    // Record a successful request; transitions HALF_OPEN → CLOSED.
    void recordSuccess();

    // Record a failed request; increments failure counter.
    // Transitions CLOSED → OPEN when threshold is reached, HALF_OPEN → OPEN.
    void recordFailure();

    State getState() const;
    int   getFailureCount() const;

    // Reset to CLOSED state with zero failure count.
    void reset();

private:
    SemaphoreHandle_t m_mutex;
    State   m_state;
    int     m_failureCount;
    int     m_failureThreshold;
    int64_t m_recoveryTimeUs;
    int64_t m_lastFailureTimeUs;
    bool    m_probeInProgress; // true while a HALF_OPEN probe request is in-flight
};

#endif // CIRCUIT_BREAKER_H
