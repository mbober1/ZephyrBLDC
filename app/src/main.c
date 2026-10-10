#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#if defined(CONFIG_UART_TRANSPORT)
#include <uart_transport.h>
UART_TRANSPORT_DEFINE(comm_uart_transport, &uart_transport_backend_api, DEVICE_DT_GET(DT_ALIAS(comm)));
#endif

#if defined(CONFIG_TELEMETRY)
#include <telemetry.h>
K_THREAD_DEFINE(telemetry_thread, CONFIG_TELEMETRY_THREAD_STACK_SIZE, telemetry_loop, NULL, NULL, NULL, CONFIG_TELEMETRY_THREAD_PRIORITY, 0, 0);
#endif

#if defined(CONFIG_COMMANDER)
#include <commander.h>
K_THREAD_DEFINE(commander_thread, CONFIG_COMMANDER_THREAD_STACK_SIZE, commander_loop, NULL, NULL, NULL, CONFIG_COMMANDER_THREAD_PRIORITY, 0, 0);
#endif

int main(void)
{
#if defined(CONFIG_UART_TRANSPORT)
	int transport_result = uart_transport_start(&comm_uart_transport);

	if (transport_result != 0) {
		LOG_ERR("Failed to start UART transport: %d", transport_result);
		return transport_result;
	}
#endif

	k_sleep(K_FOREVER);
	return 0;
}
