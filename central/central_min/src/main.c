#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "custom_client.h"
#include "sync_controller.h"

int main(void)
{
	int err;

	printk("Booting central_min\n");

	err = sync_controller_init();
	if (err) {
		printk("Synchronization output init failed: %d\n", err);
		return 0;
	}

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed: %d\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	err = custom_client_start();
	if (err) {
		printk("Client start failed: %d\n", err);
		return 0;
	}

	while (true) {
		k_sleep(K_SECONDS(10));
	}
}
