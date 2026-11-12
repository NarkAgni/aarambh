#include "support/efi_mock.h"

#include <stdlib.h>

#include "support/test.h"

EFI_BOOT_SERVICES *BS;
EFI_RUNTIME_SERVICES *RT;
EFI_SYSTEM_TABLE *ST;

EFI_STATUS mock_set_variable_status = EFI_SUCCESS;
int mock_set_variable_calls;
int test_failures;

static EFI_BOOT_SERVICES boot_services;
static EFI_RUNTIME_SERVICES runtime_services;
static int allocations_left = -1;

static EFI_STATUS EFIAPI allocate_pool(EFI_MEMORY_TYPE type, UINTN size, VOID **buffer)
{
    if (allocations_left == 0) {
        *buffer = NULL;
        return EFI_OUT_OF_RESOURCES;
    }

    if (allocations_left > 0) {
        allocations_left--;
    }

    *buffer = malloc(size);
    return *buffer != NULL ? EFI_SUCCESS : EFI_OUT_OF_RESOURCES;
}

static EFI_STATUS EFIAPI free_pool(VOID *buffer)
{
    free(buffer);
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI set_variable(CHAR16 *name, EFI_GUID *vendor, UINT32 attributes,
                                      UINTN size, VOID *data)
{
    mock_set_variable_calls++;
    return mock_set_variable_status;
}

void mock_efi_init(void)
{
    boot_services.AllocatePool = allocate_pool;
    boot_services.FreePool = free_pool;
    runtime_services.SetVariable = set_variable;

    BS = &boot_services;
    RT = &runtime_services;
}

void mock_fail_allocations_from(int number)
{
    allocations_left = number;
}

void mock_allow_allocations(void)
{
    allocations_left = -1;
}

int test_result(void)
{
    if (test_failures > 0) {
        printf("  %d check(s) failed\n", test_failures);
        return 1;
    }
    return 0;
}
