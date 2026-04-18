/*
 * Dongle connection indicator
 * Shows how many keyboard halves are connected via the XIAO's built-in LED:
 *   0 connected: RED (searching)
 *   1 connected: RED
 *   2 connected: BLUE for 60 seconds, then LED off
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/split_peripheral_status_changed.h>

LOG_MODULE_REGISTER(dongle_indicator, CONFIG_ZMK_LOG_LEVEL);

#define LED_GPIO_NODE_ID DT_COMPAT_GET_ANY_STATUS_OKAY(gpio_leds)

static const struct device *led_dev = DEVICE_DT_GET(LED_GPIO_NODE_ID);
static const uint8_t led_red_idx = DT_NODE_CHILD_IDX(DT_ALIAS(led_red));
static const uint8_t led_green_idx = DT_NODE_CHILD_IDX(DT_ALIAS(led_green));
static const uint8_t led_blue_idx = DT_NODE_CHILD_IDX(DT_ALIAS(led_blue));

static uint8_t connected_count = 0;
static struct k_work_delayable led_off_work;

static void all_leds_off(void) {
    led_off(led_dev, led_red_idx);
    led_off(led_dev, led_green_idx);
    led_off(led_dev, led_blue_idx);
}

static void set_led_red(void) {
    led_on(led_dev, led_red_idx);
    led_off(led_dev, led_green_idx);
    led_off(led_dev, led_blue_idx);
}

static void set_led_blue(void) {
    led_off(led_dev, led_red_idx);
    led_off(led_dev, led_green_idx);
    led_on(led_dev, led_blue_idx);
}

static void led_off_work_handler(struct k_work *work) {
    if (connected_count >= 2) {
        LOG_INF("Both halves connected for 60s, turning LED off");
        all_leds_off();
    }
}

static void update_led(void) {
    k_work_cancel_delayable(&led_off_work);

    if (connected_count == 0) {
        all_leds_off();
    } else if (connected_count == 1) {
        set_led_red();
    } else {
        set_led_blue();
        k_work_schedule(&led_off_work, K_SECONDS(60));
    }
}

static int peripheral_status_cb(const zmk_event_t *eh) {
    const struct zmk_split_peripheral_status_changed *ev =
        as_zmk_split_peripheral_status_changed(eh);

    if (ev->connected) {
        if (connected_count < CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS) {
            connected_count++;
        }
    } else {
        if (connected_count > 0) {
            connected_count--;
        }
    }

    LOG_INF("Peripheral %s, connected count: %d",
            ev->connected ? "connected" : "disconnected", connected_count);

    update_led();
    return 0;
}

ZMK_LISTENER(dongle_indicator, peripheral_status_cb);
ZMK_SUBSCRIPTION(dongle_indicator, zmk_split_peripheral_status_changed);

static int dongle_indicator_init(void) {
    if (!device_is_ready(led_dev)) {
        LOG_ERR("LED device not ready");
        return -ENODEV;
    }

    k_work_init_delayable(&led_off_work, led_off_work_handler);
    set_led_red();

    LOG_INF("Dongle indicator initialized, LED red (searching for halves)");
    return 0;
}

SYS_INIT(dongle_indicator_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
