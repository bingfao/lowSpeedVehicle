/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#include "shell_port.h"

#include <FreeRTOS.h>
#include <task.h>

#include "console.h"
#include "elog.h"
#include "fault.h"
#include "shell.h"
#include "user_os.h"
#include "util.h"
/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Private constants                                          ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Private macro                                              ********
 * ****************************************************************************
 */
#define BUFFER_SIZE 1024
/*
 * ****************************************************************************
 * ******** Private global variables                                   ********
 * ****************************************************************************
 */

static Shell shell_data;
static Shell *shell = &shell_data;
static char buffer[BUFFER_SIZE];
/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static short shell_port_read(char *data, unsigned short size);
static short shell_port_write(char *data, unsigned short size);
static void elog_output_func(const char *log, size_t size);
static void shelltask_handle(void const *argument);

USER_THREAD_OBJ_t g_shell_port_thread =
    USER_THREAD_OBJ_INIT(shelltask_handle, "shell_p", 1024, NULL, RTOS_PRIORITY_LOW);
/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */
void shelltask_handle(void const *argument)
{
    shellTask((void *)argument);
}

int32_t shell_port_init(void)
{
    shell_data.read = shell_port_read;
    shell_data.write = shell_port_write;

    shellInit(shell, buffer, BUFFER_SIZE);
    /* 启动elog */
    logger_startup(elog_output_func);
    g_shell_port_thread.parameter = shell;
    USER_THREAD_OBJ_t *thread = &g_shell_port_thread;
    xTaskCreate((TaskFunction_t)thread->thread, thread->name, thread->stack_size, thread->parameter, thread->priority,
                &thread->thread_handle);
    fault_assert(thread->thread_handle != NULL, FAULT_CODE_NORMAL);

    return 0;
}
/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */
/**
 * @brief: shell 串口读函数
 * @param {char} *
 * @param {unsigned} short
 * @result:
 */
static short shell_port_read(char *data, unsigned short size)
{
    return (short)console_read((uint8_t *)data, size);
}

/**
 * @brief: shell 串口写函数
 * @param {char} *
 * @param {unsigned} short
 * @result:
 */
static short shell_port_write(char *data, unsigned short size)
{
    return (short)console_write((const uint8_t *)data, size);
}

/*!
 * 嵌入式elog输出通道
 * @param log
 * @param size
 */
static void elog_output_func(const char *log, size_t size)
{
    shellWriteEndLine(shell, (char *)log, (int)size);
}
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
