#include <FreeRTOS.h>
#include <elog.h>
#include <stdio.h>
#include <stdlib.h>

#include "impella_shell.h"
#include "util.h"

#define PERM_DEBUG (0x01 << 0)
#define PERM_APP   (0x01 << 1)

/*!
 * 设置logger全局过了等级
 * 0 - 显示警告及错误，其他过滤
 * 1 - 显示信息及以上，其他过滤
 * 2 - 显示调试及以上，其他过滤
 */
static int exec_logger_globe_filter_level(int argc, char *argv[])
{
    ARGC_CHECK(argc, 1, "please input filter level");

    int level = strtol(argv[1], NULL, 10);
    switch (level) {
        case 0:
            elog_set_filter_lvl(ELOG_LVL_WARN);
            shellPrint(shellGetCurrent(), "Set logger filter level to WARNING\n\r");
            break;
        case 1:
            elog_set_filter_lvl(ELOG_LVL_INFO);
            shellPrint(shellGetCurrent(), "Set logger filter level to INF\n\r");
            break;
        case 2:
            elog_set_filter_lvl(ELOG_LVL_DEBUG);
            shellPrint(shellGetCurrent(), "Set logger filter level to DEBUG\n\r");
            break;
        default:
            shellPrint(shellGetCurrent(), "Unknown filter level %d\n\r", level);
            break;
    }

    return 0;
}

static int exec_logger_tag_filter(int argc, char *argv[])
{
    ARGC_CHECK(argc, 1, "please input tag");

    elog_set_filter_tag(argv[1]);

    return 0;
}

static int exec_logger_tag_filter_level(int argc, char *argv[])
{
    ARGC_CHECK(argc, 2, "please input tag and level");

    int level = strtol(argv[2], NULL, 10);
    switch (level) {
        case 0:
            elog_set_filter_tag_lvl(argv[1], ELOG_LVL_WARN);
            shellPrint(shellGetCurrent(), "Set logger %s level to WARNING\n\r", argv[1]);
            break;
        case 1:
            elog_set_filter_tag_lvl(argv[1], ELOG_LVL_INFO);
            shellPrint(shellGetCurrent(), "Set logger %s level to INF\n\r", argv[1]);
            break;
        case 2:
            elog_set_filter_tag_lvl(argv[1], ELOG_LVL_DEBUG);
            shellPrint(shellGetCurrent(), "Set logger %s level to DEBUG\n\r", argv[1]);
            break;
        default:
            shellPrint(shellGetCurrent(), "Unknown filter level %d\n\r", level);
            break;
    }

    return 0;
}

ShellCommand group_logger[] = {
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, global_level, exec_logger_globe_filter_level,
                         <0 : warning 1 : inf 2 : debug>),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, tag, exec_logger_tag_filter, <tag> open logger of tag),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, tag_level, exec_logger_tag_filter_level,
                         <tag><0 : warning 1 : inf 2 : debug> set logger level of tag),
    SHELL_CMD_GROUP_END()};
SHELL_EXPORT_CMD_GROUP(SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), logger, group_logger, logger commands);
