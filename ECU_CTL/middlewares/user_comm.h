/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __USER_COMM_H
#define __USER_COMM_H
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
#include "stdint.h"
#include <assert.h>
#include <string.h>

/*
 * ****************************************************************************
 * ******** Exported Types                                             ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Exported constants                                         ********
 * ****************************************************************************
 */
#define USER_OK                 (0)
#define USER_ERROR              (-1)
#define USER_ERROR_STATE        (-2)
#define USER_ERROR_DATA         (-3)
#define USER_ERROR_PARAM        (-4)
#define USER_ERROR_TIME_OUT     (-5)
#define USER_ERROR_SEND         (-6)
#define USER_ERROR_NO_MATCH_STR (-7)

/*
 * ****************************************************************************
 * ******** Exported macro                                             ********
 * ****************************************************************************
 */
#ifndef MIN
/**
 * @brief Obtain the minimum of two values.
 *
 * @note Arguments are evaluated twice. Use Z_MIN for a GCC-only, single
 * evaluation version
 *
 * @param a First value.
 * @param b Second value.
 *
 * @returns Minimum value of @p a and @p b.
 */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define HEX_TO_DEC(x) (x - '0')
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
uint32_t user_get_ticket(void);
uint16_t swap_uint16(uint16_t value);
uint32_t swap_uint32(uint32_t value);
void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);

/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /* __USER_COMM_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
