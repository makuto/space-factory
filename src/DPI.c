// Put here so that windows.h doesn't redefine a bunch of stuff
#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* #define DPI_AWARENESS_CONTEXT_SYSTEM_AWARE 3 */
/* extern BOOL SetProcessDpiAwarenessContext(int value); */
#endif

void SetDPIAware()
{
#ifdef WINDOWS
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
#endif
}
