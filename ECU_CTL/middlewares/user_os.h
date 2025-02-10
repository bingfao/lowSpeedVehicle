/*
 * @Author: your name
 * @Date: 2025-01-22 10:29:06
 * @LastEditTime: 2025-02-10 14:06:39
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\middlewares\user_os.h
 */

/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __USER_OS_H
#define __USER_OS_H
/*
 * ============================================================================
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ============================================================================
 */
#ifdef __cplusplus
extern "C"
{
#endif
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include <timers.h>
#include <semphr.h>

/*
 * ****************************************************************************
 * ******** Exported Types                                             ********
 * ****************************************************************************
 */
#define OS_MS(_ms)           ((_ms)/portTICK_PERIOD_MS)
#define OS_SECOND(second)   OS_MS(second*1000)
#define OS_NO_WAIT          OS_MS(0)
#define OS_WAIT_FOREVER     (( TickType_t )-1)
#define OS_SUCCESS          (pdTRUE)
#define OS_FAIL             (pdFALSE)

enum
{
    RTOS_PRIORITY_MIN         = 0,
    RTOS_PRIORITY_LOW         = (configMAX_PRIORITIES * 1 / 7),
    RTOS_PRIORITY_BELOWNORMAL = (configMAX_PRIORITIES * 2 / 7),
    RTOS_PRIORITY_NORMAL      = (configMAX_PRIORITIES * 3 / 7),
    RTOS_PRIORITY_ABOVENORMAL = (configMAX_PRIORITIES * 4 / 7),
    RTOS_PRIORITY_HIGH        = (configMAX_PRIORITIES * 5 / 7),
    RTOS_PRIORITY_REALTIME    = (configMAX_PRIORITIES * 6 / 7),
    RTOS_PRIORITY_MAX         = configMAX_PRIORITIES - 1
};


#pragma pack(push, 4)
typedef struct
{
    void (*thread)(void const *);
    const char *name;
    uint32_t stack_size;
    void *parameter;
    uint8_t priority;
    StackType_t *thread_stack;
    StaticTask_t tcb;
    TaskHandle_t thread_handle;
} USER_THREAD_OBJ_t;
#pragma pack(pop)

#define SET_THREAD_STACK(thread_name, word_size) static StackType_t thread_name##_stack[word_size]
#define GET_THREAD_STACK(thread_name) thread_name##_stack

#define USER_THREAD_OBJ_INIT(thread, name, stack_size, parameter, priority, thread_stack) \
    {thread, name, stack_size, parameter, priority, thread_stack, \
    {0}, NULL}

typedef struct
{
    StaticSemaphore_t sem_buf;
    SemaphoreHandle_t sem_handle;
} USER_SEM_OBJ_t;

typedef struct
{
    StaticSemaphore_t mutex_buf;
    SemaphoreHandle_t mutex_handle;
} USER_MUTEX_OBJ_T;

typedef struct
{
    StaticTimer_t timer_buf;
    TimerHandle_t timer_handle;
    char *name;
    uint32_t period;
    uint32_t auto_reload;
} USER_TIMER_OBJ_t;
/*
 * ****************************************************************************
 * ******** Exported constants                                         ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Exported macro                                             ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Exported variables                                         ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Exported Function                                          ********
 * ****************************************************************************
 */

/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__USER_OS_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
