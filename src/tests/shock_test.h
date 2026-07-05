#ifndef SHOCK_TEST_H_
#define SHOCK_TEST_H_

/**
 * @brief Run the shock detector bringup test.
 *
 * Arms the shock detector, waits 5 seconds for manual shaking,
 * then reports how many events were detected. At least 1 event
 * proves the full interrupt → queue → thread path is working.
 *
 * Only compiled in when CONFIG_BRINGUP_SELFTEST is defined.
 */
void run_shock_test(void);

#endif /* SHOCK_TEST_H_ */