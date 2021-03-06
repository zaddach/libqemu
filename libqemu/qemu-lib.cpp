

#include <llvm/Support/raw_ostream.h>

extern "C" {
#include "qemu-common.h"
#include <exec/cpu-common.h>
#include <sysemu/cpus.h>
#include <qemu/main-loop.h>
#include <qemu/error-report.h>
#include <qemu.h>
#include "cpu.h"
#include <translate-all.h>
#include <tcg/tcg.h>
}
#include <libqemu/qemu-lib-external.h>
#include <llvm/IR/Function.h>
#include <llvm/Analysis/Verifier.h>
#include <libqemu/tcg-llvm.h>

int singlestep;
unsigned long mmap_min_addr;
THREAD CPUState *thread_cpu;
CPUArchState *env;
unsigned long reserved_va;
libqemu_load_handler *libqemu_ld = NULL;
libqemu_store_handler *libqemu_st = NULL;
static TCGLLVMContext *tcg_llvm_ctx = NULL;
unsigned long guest_base = 0;
int have_guest_base = 0;

int libqemu_init(libqemu_load_handler *ld_handler, libqemu_store_handler *st_handler)
{
    const char *cpu_model = (const char *) NULL;
    CPUState *cpu;
    
    libqemu_ld = ld_handler;
    libqemu_st = st_handler; 

    error_set_progname("libqemu");
    qemu_init_exec_dir("/tmp");

    module_call_init(MODULE_INIT_QOM);

#if defined(cpudef_setup)
    cpudef_setup(); /* parse cpu definitions in target config file (TBD) */
#endif
    if (cpu_model == NULL) {
#if defined(TARGET_I386)
#ifdef TARGET_X86_64
        cpu_model = "qemu64";
#else
        cpu_model = "qemu32";
#endif
#elif defined(TARGET_ARM)
        cpu_model = "any";
#elif defined(TARGET_UNICORE32)
        cpu_model = "any";
#elif defined(TARGET_M68K)
        cpu_model = "any";
#elif defined(TARGET_SPARC)
#ifdef TARGET_SPARC64
        cpu_model = "TI UltraSparc II";
#else
        cpu_model = "Fujitsu MB86904";
#endif
#elif defined(TARGET_MIPS)
#if defined(TARGET_ABI_MIPSN32) || defined(TARGET_ABI_MIPSN64)
        cpu_model = "5KEf";
#else
        cpu_model = "24Kf";
#endif
#elif defined TARGET_OPENRISC
        cpu_model = "or1200";
#elif defined(TARGET_PPC)
# ifdef TARGET_PPC64
        cpu_model = "POWER7";
# else
        cpu_model = "750";
# endif
#elif defined TARGET_SH4
        cpu_model = TYPE_SH7785_CPU;
#else
        cpu_model = "any";
#endif
    }
    tcg_exec_init(0);
    /* NOTE: we need to init the CPU at this stage to get
       qemu_host_page_size */
    cpu = cpu_init(cpu_model);
    if (!cpu) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(EXIT_FAILURE);
    }
    env = (CPUArchState *) cpu->env_ptr;
    cpu_reset(cpu);

    thread_cpu = cpu;

    /*
     * Now that page sizes are configured in cpu_init() we can do
     * proper page alignment for guest_base.
     */
//    guest_base = HOST_PAGE_ALIGN(guest_base);
//
//    if (reserved_va || have_guest_base) {
//        guest_base = init_guest_space(guest_base, reserved_va, 0,
//                                      have_guest_base);
//        if (guest_base == (unsigned long)-1) {
//            fprintf(stderr, "Unable to reserve 0x%lx bytes of virtual address "
//                    "space for use as guest address space (check your virtual "
//                    "memory ulimit setting or reserve less using -R option)\n",
//                    reserved_va);
//            exit(EXIT_FAILURE);
//        }
//
//        if (reserved_va) {
//            mmap_next_start = reserved_va;
//        }
//    }

    /* Now that we've loaded the binary, GUEST_BASE is fixed.  Delay
       generating the prologue until now so that the prologue can take
       the real value of GUEST_BASE into account.  */
    tcg_prologue_init(&tcg_ctx);
    tcg_llvm_ctx = tcg_llvm_initialize();
    
    qemu_set_log_filename("/tmp/qemu.log");
    
    qemu_set_log(qemu_str_to_log_mask("in_asm,op"));
    return 0;
}

LLVMModuleRef libqemu_get_module(void)
{
    assert(false);
    return NULL;
}

/* Allocate a new translation block. Flush the translation buffer if
   too many translation blocks or too much generated code. */
static TranslationBlock *tb_alloc(target_ulong pc)
{
    TranslationBlock *tb;

    if (tcg_ctx.tb_ctx.nb_tbs >= tcg_ctx.code_gen_max_blocks) {
        return NULL;
    }
    tb = &tcg_ctx.tb_ctx.tbs[tcg_ctx.tb_ctx.nb_tbs++];
    tb->pc = pc;
    tb->cflags = 0;
    return tb;
}

LLVMValueRef libqemu_gen_intermediate_code(uint64_t pc, uint64_t flags, uint64_t cflags, bool single_inst)
{
    TranslationBlock *tb;
    int max_cycles = CF_COUNT_MASK;
    llvm::Function *function;
    tcg_insn_unit *gen_code_buf;

    singlestep = single_inst;

    /* Should never happen.
       We only end up here when an existing TB is too long.  */
    if (max_cycles > CF_COUNT_MASK)
        max_cycles = CF_COUNT_MASK;

    
    tb = tb_alloc(pc);
    if (unlikely(!tb)) {
        /* flush must be done */
        tb_flush(thread_cpu);
        /* cannot fail at this point */
        tb = tb_alloc(pc);
        assert(tb != NULL);
        /* Don't forget to invalidate previous TB info.  */
        tcg_ctx.tb_ctx.tb_invalidated_flag = 1;
    }

    gen_code_buf = (tcg_insn_unit *) tcg_ctx.code_gen_ptr;
    tb->tc_ptr = gen_code_buf;
    tb->cs_base = 0;
    tb->flags = flags;
    tb->cflags = cflags;
    tcg_func_start(&tcg_ctx);

    gen_intermediate_code(env, tb);

    tcg_dump_ops(&tcg_ctx);

    tcg_llvm_gen_code(tcg_llvm_ctx, &tcg_ctx, tb);
    function = static_cast<TCGPluginTBData *>(tb->opaque)->llvm_function;

    if (llvm::verifyFunction(*function, llvm::AbortProcessAction)) {
        llvm::errs() << "Failed to verify module" << '\n';
    }

    llvm::errs() << *function << '\n';
    /* TODO: Generate LLVM here */
    tb_free(tb);
    return NULL;
}
