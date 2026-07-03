#ifndef FLASH_TEST_H_
#define FLASH_TEST_H_

/**
 * @brief Run the full flash erase/write/read self-test sequence.
 *
 * Tests: JEDEC ID, usage tracking, erase verification,
 * multi-pattern writes (0xDEADBEEF, 0x12345678), single-byte write.
 * Leaves the test sector erased on exit.
 *
 * Only compiled in when CONFIG_FLASH_STORAGE_SELFTEST is defined.
 */
void run_flash_selftest(void);

#endif /* FLASH_TEST_H_ */