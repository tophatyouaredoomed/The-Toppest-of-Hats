#ifndef __DEBUG_H__
#define __DEBUG_H__

/*
 * Simple debug messages. Add:
 *
 *    #define DEBUG_LEVEL <n>
 *    #include "debug.h"
 *
 * to the file you are working on, or let DEBUG_LEVEL undefined to
 * disable all messages. A higher level denotes a greater importance
 * of the message.
 */

#include <glib.h>

# ifdef DEBUG_LEVEL
#    define DEBUG_MSG(level, fmt...) \
        ( (DEBUG_LEVEL) && ((level) >= DEBUG_LEVEL) ) ? \
        g_print(fmt) : (level)
# else
#    define DEBUG_MSG(level, fmt...)
# endif /* DEBUG_LEVEL */


#define DEBUG_HTML_WARN 1

# ifdef DEBUG_HTML_WARN
#    define DEBUG_HTML_MSG(fmt...) g_print("HTML warning: " fmt)
# else
#    define DEBUG_HTML_MSG(fmt...)
# endif /* DEBUG_HTML_WARN */


#define DEBUG_HTTP_MSG(fmt...) g_print("HTTP warning: " fmt)

#endif /* __DEBUG_H__ */


