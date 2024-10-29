/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#include "driver_com.h"

#include "error_code.h"

/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */
typedef struct
{
    DRIVER_OBJ_t driver_obj[DRIVER_NUM_MAX];
    int32_t index;
} DRIVER_LIST_t;

typedef struct
{
    fn_ptr_t fn[DRIVER_NUM_MAX];
    int32_t index;
} DRIVER_REGISTER_PREPARE_t;
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

/*
 * ****************************************************************************
 * ******** Private global variables                                   ********
 * ****************************************************************************
 */
DRIVER_LIST_t g_driver_list = {0};
DRIVER_REGISTER_PREPARE_t g_driver_register_prepare = {0};

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */

/**
 * @brief register driver function prepare, the function will be called before main function.
 * @param fn
 * @note use DRIVER_REGISTER macro to register driver function prepare.
 */
void driver_register_fun_prepare(fn_ptr_t fn)
{
    if (fn == NULL) {
        return;
    }
    if (g_driver_register_prepare.index < DRIVER_NUM_MAX) {
        g_driver_register_prepare.fn[g_driver_register_prepare.index] = fn;
        g_driver_register_prepare.index++;
    }
}

/**
 * @brief this function will be called in start function, it will call all driver function prepare to register driver
 * function.
 */
void driver_register_fun_doing(void)
{
    int32_t i;

    for (i = 0; i < g_driver_register_prepare.index; i++) {
        if (g_driver_register_prepare.fn[i]) {
            g_driver_register_prepare.fn[i]();
        }
    }
}
/**
 * @brief register driver to driver manager
 * @param driver
 * @param name
 * @return int32_t
 */
int32_t register_driver(DRIVER_CTL_t *driver, char *name_in)
{
    DRIVER_OBJ_t *p_driver_obj;

    if (driver == NULL || name_in == NULL) {
        return -EINVAL;
    }
    if (g_driver_list.index < DRIVER_NUM_MAX) {
        p_driver_obj = &g_driver_list.driver_obj[g_driver_list.index];
        memset(p_driver_obj, 0, sizeof(DRIVER_OBJ_t));
        p_driver_obj->driver = driver;
        driver->drv_obj = p_driver_obj;
        strncpy(p_driver_obj->name, name_in, DRIVER_NAME_SIZE);
        g_driver_list.index++;
    } else {
        return -ENOSR;
    }

    return 0;
}

DRIVER_OBJ_t *get_driver(const char *name)
{
    DRIVER_OBJ_t *p_driver_obj = NULL;
    int32_t i;

    if (name == NULL) {
        return NULL;
    }
    for (i = 0; i < g_driver_list.index; i++) {
        if (strncmp(g_driver_list.driver_obj[i].name, name, DRIVER_NAME_SIZE) == 0) {
            p_driver_obj = &g_driver_list.driver_obj[i];
            break;
        }
    }

    return p_driver_obj;
}

int32_t driver_init(DRIVER_OBJ_t *drv)
{
    int32_t ret = 0;

    if (drv == NULL || drv->driver == NULL || drv->driver->init == NULL) {
        return -EINVAL;
    }
    ret = drv->driver->init(drv);
    if (ret != 0) {
        return ret;
    }
    DRIVER_STATES_SET(drv, DRIVER_STATES_INITED);

    return 0;
}

int32_t driver_open(DRIVER_OBJ_t *drv, uint32_t oflag)
{
    int32_t ret = 0;

    if (drv == NULL || drv->driver == NULL || drv->driver->open == NULL) {
        return -EINVAL;
    }
    if (!DRIVER_STATES_IS(drv, DRIVER_STATES_INITED)) {
        return -EAGAIN;
    }
    if (DRIVER_STATES_IS(drv, DRIVER_STATES_OPENED)) {
        return -EALREADY;
    }
    ret = drv->driver->open(drv, oflag);
    if (ret != 0) {
        return ret;
    }
    DRIVER_STATES_SET(drv, DRIVER_STATES_OPENED);

    return 0;
}

int32_t driver_close(DRIVER_OBJ_t *drv)
{
    int32_t ret = 0;

    if (drv == NULL || drv->driver == NULL || drv->driver->close == NULL) {
        return -EINVAL;
    }
    ret = drv->driver->close(drv);
    if (ret != 0) {
        return ret;
    }
    DRIVER_STATES_CLR(drv, DRIVER_STATES_OPENED);

    return 0;
}

int32_t driver_read(DRIVER_OBJ_t *drv, uint32_t pos, void *buffer, uint32_t size)
{
    if (drv == NULL || drv->driver == NULL || drv->driver->read == NULL) {
        return -EINVAL;
    }
    if (!DRIVER_STATES_IS(drv, DRIVER_STATES_OPENED)) {
        return -EAGAIN;
    }
    return drv->driver->read(drv, pos, buffer, size);
}

int32_t driver_write(DRIVER_OBJ_t *drv, uint32_t pos, void *buffer, uint32_t size)
{
    if (drv == NULL || drv->driver == NULL || drv->driver->write == NULL) {
        return -EINVAL;
    }
    if (!DRIVER_STATES_IS(drv, DRIVER_STATES_OPENED)) {
        return -EAGAIN;
    }
    return drv->driver->write(drv, pos, buffer, size);
}

int32_t driver_control(DRIVER_OBJ_t *drv, int cmd, void *args)
{
    if (drv == NULL || drv->driver == NULL || drv->driver->control == NULL) {
        return -EINVAL;
    }
    if (!DRIVER_STATES_IS(drv, DRIVER_STATES_OPENED)) {
        return -EAGAIN;
    }
    return drv->driver->control(drv, cmd, args);
}

int32_t get_driver_status(DRIVER_OBJ_t *drv)
{
    if (drv == NULL) {
        return -EINVAL;
    }
    return drv->states;
}

void driver_set_inted(DRIVER_OBJ_t *drv)
{
    DRIVER_STATES_SET(drv, DRIVER_STATES_INITED);
}

void driver_clear_inted(DRIVER_OBJ_t *drv)
{
    DRIVER_STATES_CLR(drv, DRIVER_STATES_INITED);
}

bool driver_is_inted(DRIVER_OBJ_t *drv)
{
    return DRIVER_STATES_IS(drv, DRIVER_STATES_INITED);
}

void driver_set_opened(DRIVER_OBJ_t *drv)
{
    DRIVER_STATES_SET(drv, DRIVER_STATES_OPENED);
}

void driver_clear_opened(DRIVER_OBJ_t *drv)
{
    DRIVER_STATES_CLR(drv, DRIVER_STATES_OPENED);
}

bool driver_is_opened(DRIVER_OBJ_t *drv)
{
    return DRIVER_STATES_IS(drv, DRIVER_STATES_OPENED);
}

void driver_set_reading(DRIVER_OBJ_t *drv)
{
    DRIVER_STATES_SET(drv, DRIVER_STATES_READING);
}

void driver_clear_reading(DRIVER_OBJ_t *drv)
{
    DRIVER_STATES_CLR(drv, DRIVER_STATES_READING);
}

bool driver_is_reading(DRIVER_OBJ_t *drv)
{
    return DRIVER_STATES_IS(drv, DRIVER_STATES_READING);
}

void driver_set_writing(DRIVER_OBJ_t *drv)
{
    DRIVER_STATES_SET(drv, DRIVER_STATES_WRITING);
}

void driver_clear_writing(DRIVER_OBJ_t *drv)
{
    DRIVER_STATES_CLR(drv, DRIVER_STATES_WRITING);
}

bool driver_is_writing(DRIVER_OBJ_t *drv)
{
    return DRIVER_STATES_IS(drv, DRIVER_STATES_WRITING);
}

/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
