/*
 * this is the internal transfer function.
 *
 * HISTORY
 * 24-Nov-02  Christian Tismer  <tismer@tismer.com>
 *      needed to add another magic constant to insure
 *      that f in slp_eval_frame(PyFrameObject *f)
 *      STACK_REFPLUS will probably be 1 in most cases.
 *      gets included into the saved stack area.
 * 26-Sep-02  Christian Tismer  <tismer@tismer.com>
 *      again as a result of virtualized stack access,
 *      the compiler used less registers. Needed to
 *      explicit mention registers in order to get them saved.
 *      Thanks to Jeff Senn for pointing this out and help.
 * 17-Sep-02  Christian Tismer  <tismer@tismer.com>
 *      after virtualizing stack save/restore, the
 *      stack size shrunk a bit. Needed to introduce
 *      an adjustment STACK_MAGIC per platform.
 * 15-Sep-02  Gerd Woetzel       <gerd.woetzel@GMD.DE>
 *      slightly changed framework for sparc
 * 01-Mar-02  Christian Tismer  <tismer@tismer.com>
 *      Initial final version after lots of iterations for i386.
 */

#define alloca _alloca

#define STACK_REFPLUS 1

#ifdef SLP_EVAL

#define STACK_MAGIC 0

/* Some magic to quell warnings and keep slp_switch() from crashing when built
   with VC90. Disable global optimizations, and the warning: frame pointer
   register 'ebp' modified by inline assembly code.

   We used to just disable global optimizations ("g") but upstream stackless
   Python, as well as stackman, turn off all optimizations.

References:
https://github.com/stackless-dev/stackman/blob/dbc72fe5207a2055e658c819fdeab9731dee78b9/stackman/platforms/switch_x86_msvc.h
https://github.com/stackless-dev/stackless/blob/main-slp/Stackless/platf/switch_x86_msvc.h
*/
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#pragma optimize("", off) /* so that autos are stored on the stack */
#pragma warning(disable:4731)
#pragma warning(disable:4733) /* disable warning about modifying FS[0] */


static int
slp_switch(void)
{
    /* MASM systax is typically reversed from other assemblers.
       It is usually <instruction> <destination> <source>
     */
    int *stackref, stsizediff;
    /* store the structured exception state for this stack */
    DWORD seh_state = __readfsdword(FIELD_OFFSET(NT_TIB, ExceptionList));
    fprintf(stderr, "With target %p saving SEH state %p\n",
            switching_thread_state,
            (void*)seh_state);
    __asm mov stackref, esp;
    /* modify EBX, ESI and EDI in order to get them preserved */
    __asm mov ebx, ebx;
    __asm xchg esi, edi;
    {
        SLP_SAVE_STATE(stackref, stsizediff);
        __asm {
            mov     eax, stsizediff
            add     esp, eax
            add     ebp, eax
        }
        SLP_RESTORE_STATE();
    }
    fprintf(stderr, "On resume with target %p, SEH state is %p; will restore to %p\n",
            switching_thread_state,
            (void*)__readfsdword(FIELD_OFFSET(NT_TIB, ExceptionList)),
            (void*)seh_state);
    __writefsdword(FIELD_OFFSET(NT_TIB, ExceptionList), seh_state);

    return 0;
}

/* re-enable ebp warning and global optimizations. */
#pragma optimize("", on)
#pragma warning(default:4731)
#pragma warning(default:4733) /* disable warning about modifying FS[0] */


#endif

/*
 * further self-processing support
 */

/* we have IsBadReadPtr available, so we can peek at objects */
#define STACKLESS_SPY

#ifdef IMPLEMENT_STACKLESSMODULE
#include "Windows.h"
#define CANNOT_READ_MEM(p, bytes) IsBadReadPtr(p, bytes)

static int IS_ON_STACK(void*p)
{
    int stackref;
    int stackbase = ((int)&stackref) & 0xfffff000;
    return (int)p >= stackbase && (int)p < stackbase + 0x00100000;
}

#endif
