
/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __DRIVER_COM_H
#define __DRIVER_COM_H
/*
 * ============================================================================
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ============================================================================
 */
#ifdef __cplusplus
extern "C" {
#endif
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * ****************************************************************************
 * ******** Exported Types                                             ********
 * ****************************************************************************
 */
#define DRIVER_NAME_SIZE            16
#define DRIVER_NUM_MAX              20

#define DRIVER_REGISTER_PREPARE_MAX DRIVER_NUM_MAX

typedef void (*fn_ptr_t)(void);
typedef struct DRIVER_CTL DRIVER_CTL_t;

typedef struct
{
    char name[DRIVER_NAME_SIZE];
    int32_t states;
    DRIVER_CTL_t *driver;
} DRIVER_OBJ_t;

struct DRIVER_CTL
{
    DRIVER_OBJ_t *drv_obj;  // driver object, point the upper-level data(DRIVER_OBJ_t)
    void *drv_priv;         // private data for driver
    int32_t (*init)(DRIVER_OBJ_t *drv);
    int32_t (*deinit)(DRIVER_OBJ_t *drv);
    int32_t (*open)(DRIVER_OBJ_t *drv, uint32_t oflag);
    int32_t (*close)(DRIVER_OBJ_t *drv);
    int32_t (*read)(DRIVER_OBJ_t *drv, uint32_t pos, void *buffer, uint32_t size);
    int32_t (*write)(DRIVER_OBJ_t *drv, uint32_t pos, void *buffer, uint32_t size);
    int32_t (*control)(DRIVER_OBJ_t *drv, uint32_t cmd, void *args);
};

typedef enum {
    DEV_POS_NONE = 0,
    DEV_RXTX_POS_BLOCKING,       // driver tx position blocking
    DEV_RXTX_POS_BLOCKING_1000,  // driver tx position blocking 1000ms
    DEV_RXTX_POS_NONBLOCKING,    // driver tx position non-blocking
} DRV_RW_POS_t;

typedef enum {
    DRV_CMD_NONE = 0,
    DRV_CMD_SET_WRITE_DONE_CALLBACK,      // set write done callback
    DRV_CMD_SET_WRITE_DONE_CALLBACK_ARG,  // set write done callback arg
    DRV_CMD_SET_READ_DONE_CALLBACK,       // set read done callback
    DRV_CMD_SET_READ_DONE_CALLBACK_ARG,   // set read done callback arg
    DRV_CMD_GET_SIZE,                     // get size of driver
    DRV_CMD_GET_ID,                       // get id of driver
    DRV_CMD_MAX,
} DRV_CMD_t;

/*
 * ****************************************************************************
 * ******** Exported constants                                         ********
 * ****************************************************************************
 */

/**
 * @brief The base CMD of drivers operation command.
 */
#define DRV_CMD_NET_PORT_OPERATION_BASE 0x0100  // 0x0100~0x01FF The base CMD of the network port operation command.

/**
 * @brief the driver states
 */
#define DRIVER_STATES_NONE              0x00000000
#define DRIVER_STATES_INITED            0x00000001
#define DRIVER_STATES_OPENED            0x00000002
#define DRIVER_STATES_WRITING           0x00000004
#define DRIVER_STATES_READING           0x00000008
#define DRIVER_STATES_ERROR             0x80000000

/*
 * ****************************************************************************
 * ******** Exported macro                                             ********
 * ****************************************************************************
 */

/**
 * @brief Definition of function register.
 *        Use "__attribute__((constructor))" to modify function. This function
 *        will be run before main() function.
 */
#define FUN_REGISTER(fun)                                                                                              \
    static void __attribute__((constructor)) fun##_register(void)                                                      \
    {                                                                                                                  \
        driver_register_fun_prepare(fun);                                                                              \
    }

/**
 * @brief Driver register
 */
#define DRIVER_REGISTER(drv_ctl, name)                                                                                 \
    static void name##_register(void)                                                                                  \
    {                                                                                                                  \
        DRIVER_CTL_t *drv = drv_ctl;                                                                                   \
        register_driver(drv, #name);                                                                                   \
    }                                                                                                                  \
    FUN_REGISTER(name##_register)

#define DRIVER_STATES_SET(drv, state) (drv->states |= state)
#define DRIVER_STATES_CLR(drv, state) (drv->states &= ~state)
#define DRIVER_STATES_IS(drv, state)  ((drv->states & state) != 0)

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
void driver_register_fun_prepare(fn_ptr_t fn);
void driver_register_fun_doing(void);

int32_t register_driver(DRIVER_CTL_t *driver, char *name);
DRIVER_OBJ_t *get_driver(const char *name);
int32_t driver_init(DRIVER_OBJ_t *drv);
int32_t driver_deinit(DRIVER_OBJ_t *drv);
int32_t driver_open(DRIVER_OBJ_t *drv, uint32_t oflag);
int32_t driver_close(DRIVER_OBJ_t *drv);
int32_t driver_read(DRIVER_OBJ_t *drv, uint32_t pos, void *buffer, uint32_t size);
int32_t driver_write(DRIVER_OBJ_t *drv, uint32_t pos, void *buffer, uint32_t size);
int32_t driver_control(DRIVER_OBJ_t *drv, int cmd, void *args);
int32_t get_driver_status(DRIVER_OBJ_t *drv);
void driver_set_inted(DRIVER_OBJ_t *drv);
void driver_clear_inted(DRIVER_OBJ_t *drv);
bool driver_is_inted(DRIVER_OBJ_t *drv);
void driver_set_opened(DRIVER_OBJ_t *drv);
void driver_clear_opened(DRIVER_OBJ_t *drv);
bool driver_is_opened(DRIVER_OBJ_t *drv);
void driver_set_reading(DRIVER_OBJ_t *drv);
void driver_clear_reading(DRIVER_OBJ_t *drv);
bool driver_is_reading(DRIVER_OBJ_t *drv);
void driver_set_writing(DRIVER_OBJ_t *drv);
void driver_clear_writing(DRIVER_OBJ_t *drv);
bool driver_is_writing(DRIVER_OBJ_t *drv);

/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__DRIVER_COM_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
