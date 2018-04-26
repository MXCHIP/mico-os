#include "include.h"
#include "rtos_pub.h"
#include "BkDriverWdg.h"

OSStatus MicoWdgInitialize( uint32_t timeout )
{
	return bk_wdg_initialize(timeout);
}

void MicoWdgReload( void )
{
	bk_wdg_reload();
}

OSStatus MicoWdgFinalize( void )
{
	return bk_wdg_finalize();
}

// eof

