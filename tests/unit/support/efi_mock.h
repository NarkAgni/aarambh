#ifndef AARAMBH_TESTS_UNIT_EFI_MOCK_H
#define AARAMBH_TESTS_UNIT_EFI_MOCK_H

#include <efi.h>
#include <efilib.h>

/* Boot and runtime services backed by malloc; call once before a test. */
void mock_efi_init(void);

/* The allocation with this number (0 is the next one) and all later ones fail. */
void mock_fail_allocations_from(int number);
void mock_allow_allocations(void);

/* What the next SetVariable call returns, and how often it was called. */
extern EFI_STATUS mock_set_variable_status;
extern int mock_set_variable_calls;

#endif
