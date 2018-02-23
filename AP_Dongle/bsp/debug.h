

#include "datatype.h"

#define DEBUG_LEVEL_HST			0
#define	DEBUG_LEVEL_ERROR		1	
#define	DEBUG_LEVEL_INFO		2	
#define	DEBUG_LEVEL_DEBUG		3

#define DEBUG_LEVEL_DFAULT		DEBUG_LEVEL_DEBUG

UINT8 Debug_GetLevel(void);
void Debug_SetLevel(UINT8 new_level);
void pdebughex(UINT8 *src, UINT16 len);
void pdebug(const char *format, ...);
void perr(const char *format, ...);
void pinfo(const char *format, ...);
void pprint(const char *format, ...);
void phex(UINT8 *src, UINT16 len);
void perrhex(UINT8 *src, UINT16 len);