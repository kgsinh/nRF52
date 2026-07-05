#ifndef SHOCK_LOG_TEST_H_
#define SHOCK_LOG_TEST_H_

/**
 * @brief Run the shock log bringup verification test.
 *
 * Mounts LittleFS, writes 3 synthetic shock entries, reads them back,
 * verifies round-trip integrity, prints filesystem stats, then clears
 * the log. Leaves the log empty on exit.
 *
 * Only compiled in when CONFIG_BRINGUP_SELFTEST is defined.
 */
void run_shock_log_test(void);

#endif /* SHOCK_LOG_TEST_H_ */