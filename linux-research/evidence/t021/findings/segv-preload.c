#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>

static void report(void* addr, const char* tag)
{
  Dl_info info;
  if (dladdr(addr, &info) && info.dli_fname)
    fprintf(stderr, "SEGV %s %p %s+0x%lx\n", tag, addr, info.dli_fname,
            (unsigned long)((char*)addr - (char*)info.dli_fbase));
  else
    fprintf(stderr, "SEGV %s %p ?\n", tag, addr);
}

static void handler(int sig, siginfo_t* si, void* uc)
{
  ucontext_t* ctx = (ucontext_t*)uc;
  fprintf(stderr, "SEGV signal %d fault-addr %p\n", sig, si->si_addr);
#if defined(__x86_64__)
  report((void*)ctx->uc_mcontext.gregs[REG_RIP], "pc");
#elif defined(__aarch64__)
  report((void*)ctx->uc_mcontext.pc, "pc");
  report((void*)ctx->uc_mcontext.regs[30], "lr");
#endif
  void* frames[40];
  int n = backtrace(frames, 40);
  for (int i = 0; i < n; i++) report(frames[i], "frame");
  _exit(128 + sig);
}

__attribute__((constructor)) static void install(void)
{
  struct sigaction sa = {0};
  sa.sa_sigaction = handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, 0);
  sigaction(SIGBUS, &sa, 0);
  sigaction(SIGFPE, &sa, 0);
}

// Who calls exit(): print the caller chain, then exit as usual.
void exit(int status)
{
  fprintf(stderr, "SEGV exit(%d) called\n", status);
  void* frames[40];
  int n = backtrace(frames, 40);
  for (int i = 0; i < n; i++) report(frames[i], "exit-frame");
  void (*real_exit)(int) = (void (*)(int))dlsym(RTLD_NEXT, "exit");
  real_exit(status);
  __builtin_unreachable();
}
