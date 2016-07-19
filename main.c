#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"
#include "driver/uart.h"

#include "monitor.h"
#include "main.h"

void ICACHE_FLASH_ATTR init_done_cb(void) {
	monitor_start();
}

void ICACHE_FLASH_ATTR user_init(void)
{
	/* Initialize UART */
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
#ifdef SERIAL_OUT_GPIO2
	os_printf("\n\nSerial Output on UART1 (GPIO2)\n");
	UART_SetPrintPort(1);
#endif

	wifi_set_opmode(STATION_MODE);
	wifi_station_set_auto_connect(0);
	wifi_station_set_reconnect_policy(false);

	os_printf("\n--- horst ESP ---\n");
	os_printf("SDK version: %s\n", system_get_sdk_version());

	system_init_done_cb(&init_done_cb);
} 
