#include "interceptor-routing-trampoline-arm64.h"
#include "closurebridge.h"
#include "interceptor.h"
#include "macros.h"
#include "memory_manager.h"

#include <stdlib.h>
#include <string.h>

#define ARM64_TINY_REDIRECT_SIZE 4
#define ARM64_FULL_REDIRECT_SIZE 16
#define ARM64_NEAR_JUMP_RANGE ((1 << 25) << 2)

void interceptor_cclass(initialize_interceptor_backend)(memory_manager_t *memory_manager) {
    if (memory_manager == NULL) {
        memory_manager = memory_manger_class(shared_instance)();
    }

    interceptor_backend_arm64_t *backend = SAFE_MALLOC_TYPE(interceptor_backend_arm64_t);
    backend->reader_arm64                = SAFE_MALLOC_TYPE(ARM64AssemblyReader);
    backend->writer_arm64                = SAFE_MALLOC_TYPE(ARM64AssemblyWriter);
    backend->relocator_arm64             = SAFE_MALLOC_TYPE(ARM64Relocator);

    backend->memory_manager = memory_manager;
}

ARCH_API void interceptor_trampoline_cclass(active)(hook_entry_t *entry) {
    ARM64HookEntryBackend *entry_backend = (ARM64HookEntryBackend *)entry->backend;

    ARM64AssemblerWriter *writer_arm64;
    writer_arm64 = arm64_assembly_writer_cclass(new)(entry->target_address);

    if (entry->type == HOOK_TYPE_FUNCTION_via_REPLACE) {
        if (entry_backend->limit_relocate_inst_size = ARM64_TINY_REDIRECT_SIZE) {
            arm64_assembly_writer_cclass(put_b_imm)(writer_arm64, (zz_addr_t)entry->on_enter_transfer_trampoline -
                                                                      (zz_addr_t)writer_arm64->start_pc);
        } else {
            arm64_assembly_writer_cclass(put_ldr_reg_imm)(writer_arm64, ARM64_REG_X17, 0x8);
            arm64_assembly_writer_cclass(put_br_reg)(writer_arm64, ARM64_REG_X17);
            arm64_assembly_writer_cclass(put_bytes)(&entry->on_enter_transfer_trampoline, sizeof(void *));
        }
    } else {
        if (entry_backend->limit_relocate_inst_size = ARM64_TINY_REDIRECT_SIZE) {
            arm64_assembly_writer_cclass(put_b_imm)(writer_arm64, (zz_addr_t)entry->on_enter_transfer_trampoline -
                                                                      (zz_addr_t)writer_arm64->start_pc);
        } else {
            arm64_assembly_writer_cclass(put_ldr_reg_imm)(writer_arm64, ARM64_REG_X17, 0x8);
            arm64_assembly_writer_cclass(put_br_reg)(writer_arm64, ARM64_REG_X17);
            arm64_assembly_writer_cclass(put_bytes)(&entry->on_enter_trampoline, sizeof(void *));
        }
    }
    arm64_assembly_writer_cclass(destory)(writer_arm64);
}

ARCH_API void interceptor_trampoline_cclass(build_for_enter_transfer)(hook_entry_t *entry) {
    ARM64HookEntryBackend *entry_backend = (ARM64HookEntryBackend *)entry->backend;
    ARM64AssemblerWriter *writer_arm64;
    writer_arm64 = arm64_assembly_writer_cclass(new)(0);

    arm64_assembly_writer_cclass(put_ldr_reg_imm)(writer_arm64, ARM64_REG_X17, 0x8);
    arm64_assembly_writer_cclass(put_br_reg)(writer_arm64, ARM64_REG_X17);
    if (entry->type == HOOK_TYPE_FUNCTION_via_REPLACE) {
        arm64_assembly_writer_cclass(put_bytes)(&entry->replace_call, sizeof(void *));
    } else if (entry->type == HOOK_TYPE_DBI) {
        arm64_assembly_writer_cclass(put_bytes)(&entry->on_dynamic_binary_instrumentation_trampoline, sizeof(void *));
    } else {
        arm64_assembly_writer_cclass(put_bytes)(&entry->on_enter_trampoline, sizeof(void *));
    }

    memory_manager_t memory_manager = memory_manger_cclass(shared_instance)();
    if (entry_backend->limit_relocate_inst_size == ARM64_TINY_REDIRECT_SIZE) {
        CodeCave *cc;
        cc = memory_manger_class(search_near_code_cave)(memory_manager, entry->target_address, ARM64_TINY_REDIRECT_SIZE,
                                                        writer_arm64->inst_bytes.size);
        CHECK(cc);
        arm64_assembly_writer_cclass(patch_to)(writer_arm64, cc->data);
        entry->on_enter_transfer_trampoline = (void *)cc->address;
        SAFE_FREE(cc);
    } else {
        CodeSlice *cs;
        cs = memory_manager_cclass(allocate_code_slice)(writer_arm64->inst_bytes.size);
        CHECK(cs);
        arm64_assembly_writer_cclass(patch_to)(writer_arm64, cs->data);
        entry->on_enter_transfer_trampoline = (void *)cs->data;
        SAFE_FREE(cs);
    }
}

ARCH_API void interceptor_trampoline_cclass(build_for_enter)(hook_entry_t *entry) {
    ARM64HookEntryBackend *entry_backend = (ARM64HookEntryBackend *)entry->backend;
    if (entry->type == HOOK_TYPE_FUNCTION_via_GOT) {
        DynamicClosureBridgeInfo *dcbInfo;
        DynamicClosureBridge *dcb = DynamicClosureBridgeCClass(shared_instance)();
        DynamicClosureBridgeCClass(AllocateDynamicClosureBridge)(dcb, (void *)context_begin_invocation_bridge_handler);
        if (dcbInfo == NULL) {
            ERROR_LOG_STR("build closure bridge failed!!!");
        }
        entry->on_enter_trampoline = dcbInfo->redirect_trampoline;
    }
    if (entry->type != HOOK_TYPE_FUNCTION_via_GOT) {
        ClosureBridgeInfo *cbInfo;
        ClosureBridge *cb = ClosureBridgeCClass(shared_instance)();
        ClosureBridgeCClass(AllocateClosureBridge)(cb, (void *)context_begin_invocation_bridge_handler);

        if (cbInfo == NULL) {
            ERROR_LOG_STR("build closure bridge failed!!!");
        }
        entry->on_enter_trampoline = cbInfo->redirect_trampoline;
    }

    // build the double trampline aka enter_transfer_trampoline
    if (entry_backend && entry_backend->limit_relocate_inst_size == ARM64_TINY_REDIRECT_SIZE) {
        if (entry->hook_type != HOOK_TYPE_FUNCTION_via_GOT) {
            interceptor_trampoline_cclass(build_for_enter_transfer)(entry);
        }
    }
}

ARCH_API void interceptor_trampoline_cclass(build_for_invoke)(hook_entry_t *entry) {
    ARM64HookEntryBackend *entry_backend = (ARM64HookEntryBackend *)entry->backend;
    zz_addr_t origin_next_inst_addr;

    ARM64AssemblyReader *reader_arm64 = arm64_assembly_reader_cclass(new)(entry->target_address, entry->target_address);
    ARM64AssemblyWriter *writer_arm64 = arm64_assembly_writer_cclass(new)(0);
    ARM64Relocator *relocator_arm64   = arm64_assembly_relocator_cclass(new)(reader_arm64, writer_arm64);

    do {
        arm64_assembly_relocator_cclass(relocate_write)(relocator_arm64);
    } while (relocator_arm64->io_indexs.size < relocator_arm64->input->instCTXs.size);

    assert(entry_backend->limit_relocate_inst_size == relocator_arm64->input->inst_bytes.size);

    origin_next_inst_addr = (zz_addr_t)entry->target_address + relocator_arm64->input->inst_bytes.size;
    arm64_assembly_writer_cclass(put_ldr_reg_imm)(writer_arm64, ARM64_REG_X17, 0x8);
    arm64_assembly_writer_cclass(put_br_reg)(writer_arm64, ARM64_REG_X17);
    arm64_assembly_writer_cclass(put_bytes)(&origin_next_inst_addr, sizeof(void *));

    memory_manager_t *memory_manager = memory_manger_cclass(shared_instance)();
    CodeSlice *cs;
    cs = memory_manager_cclass(allocate_code_slice)(relocator_arm64->output->inst_bytes.size);
    CHECK(cs);

    arm64_assembly_relocator_cclass(double_write)(relocator_arm64, cs->data);
    arm64_assembly_writer_cclass(patch_to)(relocator_arm64->output, cs->data, relocator_arm64->output->inst_bytes->data,
                                           relocator_arm64->output->inst_bytes.size);
    entry->on_enter_transfer_trampoline = (void *)cs->data;
    SAFE_FREE(cs);
}

ARCH_API void interceptor_trampoline_cclass(build_for_leave)(hook_entry_t *entry) {

    ARM64HookEntryBackend *entry_backend = (ARM64HookEntryBackend *)entry->backend;
    if (entry->type == HOOK_TYPE_FUNCTION_via_GOT) {
        DynamicClosureBridgeInfo *dcbInfo;
        DynamicClosureBridge *dcb = DynamicClosureBridgeCClass(shared_instance)();
        dcbInfo                   = DynamicClosureBridgeCClass(AllocateDynamicClosureBridge)(
            dcb, (void *)dynamic_context_end_invocation_bridge_handler);
        if (dcbInfo == NULL) {
            ERROR_LOG_STR("build closure bridge failed!!!");
        }
        entry->on_enter_trampoline = dcbInfo->redirect_trampoline;
    }
    if (entry->type != HOOK_TYPE_FUNCTION_via_GOT) {
        ClosureBridgeInfo *cbInfo;
        ClosureBridge *cb = ClosureBridgeCClass(shared_instance)();
        cbInfo = ClosureBridgeCClass(AllocateClosureBridge)(cb, (void *)context_end_invocation_bridge_handler);

        if (cbInfo == NULL) {
            ERROR_LOG_STR("build closure bridge failed!!!");
        }
        entry->on_leave_trampoline = cbInfo->redirect_trampoline;
    }
}

ARCH_API void interceptor_trampoline_cclass(build_for_dynamic_binary_instrumentation)(hook_entry_t *entry) {
    ARM64HookEntryBackend *entry_backend = (ARM64HookEntryBackend *)entry->backend;
    ClosureBridgeInfo *cbInfo;
    ClosureBridge *cb = ClosureBridgeCClass(shared_instance)();
    cbInfo            = ClosureBridgeCClass(AllocateClosureBridge)(cb, (void *)context_end_invocation_bridge_handler);

    if (cbInfo == NULL) {
        ERROR_LOG_STR("build closure bridge failed!!!");
    }

    entry->on_dynamic_binary_instrumentation_trampoline = cbInfo->redirect_trampoline;

    // build the double trampline aka enter_transfer_trampoline
    if (entry_backend->limit_relocate_inst_size == ARM64_TINY_REDIRECT_SIZE) {
        if (entry->hook_type != HOOK_TYPE_FUNCTION_via_GOT) {
            interceptor_trampoline_cclass(build_for_enter_transfer)(entry);
        }
    }
}
