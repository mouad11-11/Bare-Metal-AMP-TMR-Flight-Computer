#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

/*
 * ==============================================================================
 * Bare-Metal AMP TMR Flight Computer - Configuration Parameters
 * All safety tunables, bounds, thresholds, and operational limits.
 * ==============================================================================
 */

/* Maximum pairwise difference (in microseconds) counted as node agreement */
#define VOTE_AGREE_THRESHOLD_US     5

/* Plausibility range of commanded actuator PWM pulse width (in microseconds) */
#define PWM_MIN_US                  1000
#define PWM_NEUTRAL_US              1500
#define PWM_MAX_US                  2000

/* Maximum allowable PWM change between consecutive frames (Rate Limiter: us/frame) */
#define PWM_MAX_STEP_US             200

/* Consecutive fault count threshold before latching a node out permanently */
#define NODE_FAULT_LATCH_N          3

/* Consecutive healthy frames required to decay fault counter by 1 (Leaky bucket) */
#define NODE_GOOD_STREAK_M          100

/* Consecutive missed frame deadlines before treating a node as permanently failed */
#define NODE_MISS_LIMIT_K           2

/* Maximum in-flight controlled resets allowed before hard latching into SAFE (0 = none) */
#define RESET_MAX_R                 0

/* Actuator-side auto-safe timeout in milliseconds if no heartbeat received */
#define ACTUATOR_TIMEOUT_MS         50

/* Allow degraded 2-out-of-2 operational mode when 1 node is latched out */
#define ALLOW_DEGRADED_2OO2         1

/* Legacy in-band fail-safe PWM command value (for telemetry backward compatibility) */
#define FAILSAFE_PWM_US             (-9999)

/* Software watchdog spin-loop timeout bound (cycles) */
#define WATCHDOG_MAX_CYCLES         2000000U

/* Stack Canary and Sizing Definitions */
#define STACK_TOTAL_SIZE            32768U
#define STACK_PER_CORE_SIZE         8192U
#define STACK_SVC_SIZE              6144U
#define STACK_CANARY_VALUE          0xDEADBEEFU
#define STACK_WATERMARK_VALUE       0xA5A5A5A5U
#define STACK_CANARY_WORDS          4U

/* Maximum number of recorded exception / fault entries per core */
#define MAX_FAULT_RECORDS_PER_CORE  4

/* Design Diversity & Sensor Triplication Groundwork (P3.2 & P3.3) */
#define DIVERSITY_ENABLED           0       /* 0 = Identical algorithms (baseline), 1 = Diverse formulation on Node 2 */
#define SENSOR_INPUT_VOTING_ENABLED 0       /* 0 = Shared single-sensor channel (baseline), 1 = Triplicate sensor voting */
#define SENSOR_VOTE_TOLERANCE       10      /* Max allowable difference between sensor channels (ddeg/s) */

#endif /* CONFIG_H */
