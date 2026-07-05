#ifndef BLE_TEST_H_
#define BLE_TEST_H_

/**
 * @brief Run the BLE bringup verification test.
 *
 * Confirms BLE stack initialized and advertising is active.
 * Waits 15 seconds for a client to connect, reports whether
 * connection succeeded. Also tests shock notification path.
 *
 * Only compiled in when CONFIG_BRINGUP_SELFTEST is defined.
 */
void run_ble_test(void);

#endif /* BLE_TEST_H_ */