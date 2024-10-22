/**
 * Impella Actuator
 * Copyright (c) 2024 xxx. All rights reserved.
 * @file   : logo.c
 * @brief  :
 * @author :
 * @date   : 2024/4/4
 */

const char *logo =
        "\r\n"
        "\033[2J\033[1H"
        "##     ## #### ##    ##    ########   #######  ##    ##  ######\r\n"
        " ##   ##   ##  ###   ##    ##     ## ##     ## ###   ## ##    ##\r\n"
        "  ## ##    ##  ####  ##    ##     ## ##     ## ####  ## ##\r\n"
        "   ###     ##  ## ## ##    ##     ## ##     ## ## ## ## ##   ####\r\n"
        "  ## ##    ##  ##  ####    ##     ## ##     ## ##  #### ##    ##\r\n"
        " ##   ##   ##  ##   ###    ##     ## ##     ## ##   ### ##    ##\r\n"
        "##     ## #### ##    ##    ########   #######  ##    ##  ######\r\n"
        "             |__|\r\n"
        "Copyright\t: (c) 2024 Dec\r\n"
        "\n\r";

const char *build =
#ifndef NDEBUG
        "Build\t\t: "__DATE__" "__TIME__"\r\n"
#endif
        "";
