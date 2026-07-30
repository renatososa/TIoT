#include <errno.h>
#include <stdbool.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "energy_markers.h"

#define MARKER_NODE DT_PATH(zephyr_user)

BUILD_ASSERT(DT_NODE_HAS_STATUS(MARKER_NODE, okay),
	     "The zephyr,user marker node is required");

static const struct gpio_dt_spec acquisition_marker =
	GPIO_DT_SPEC_GET(MARKER_NODE, acquisition_marker_gpios);
static const struct gpio_dt_spec transmitter_marker =
	GPIO_DT_SPEC_GET(MARKER_NODE, transmitter_marker_gpios);
static const struct gpio_dt_spec advertising_marker =
	GPIO_DT_SPEC_GET(MARKER_NODE, advertising_marker_gpios);
static const struct gpio_dt_spec connected_marker =
	GPIO_DT_SPEC_GET(MARKER_NODE, connected_marker_gpios);
static const struct gpio_dt_spec profile_marker =
	GPIO_DT_SPEC_GET(MARKER_NODE, profile_marker_gpios);

static int configure_output(const struct gpio_dt_spec *marker)
{
	if (!gpio_is_ready_dt(marker)) {
		return -ENODEV;
	}

	return gpio_pin_configure_dt(marker, GPIO_OUTPUT_INACTIVE);
}

int energy_markers_init(void)
{
	const struct gpio_dt_spec *markers[] = {
		&acquisition_marker,
		&transmitter_marker,
		&advertising_marker,
		&connected_marker,
		&profile_marker,
	};

	for (size_t index = 0U; index < ARRAY_SIZE(markers); index++) {
		int err = configure_output(markers[index]);

		if (err) {
			return err;
		}
	}

	return 0;
}

void energy_markers_advertising(bool active)
{
	gpio_pin_set_dt(&advertising_marker, active);
}

void energy_markers_connected(bool active)
{
	gpio_pin_set_dt(&connected_marker, active);
}

void energy_markers_profile(bool active)
{
	gpio_pin_set_dt(&profile_marker, active);
}

void energy_markers_acquisition(bool active)
{
	gpio_pin_set_dt(&acquisition_marker, active);
}

void energy_markers_transmitter(bool active)
{
	gpio_pin_set_dt(&transmitter_marker, active);
}

void energy_markers_all_off(void)
{
	energy_markers_acquisition(false);
	energy_markers_transmitter(false);
	energy_markers_profile(false);
	energy_markers_connected(false);
	energy_markers_advertising(false);
}
