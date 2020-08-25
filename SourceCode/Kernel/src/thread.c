//
// Created by XingfengYang on 2020/6/26.
//

#include <kheap.h>
#include <kstack.h>
#include <kvector.h>
#include <log.h>
#include <stdlib.h>
#include <string.h>
#include <thread.h>
#include <vfs_dentry.h>

extern Heap kernelHeap;

extern uint64_t ktimer_sys_runtime();

uint32_t pidMap[2048] = {0};

uint32_t thread_alloc_pid() {
  for (uint32_t i = 0; i < 2048 / BITS_IN_UINT32; i++) {
    if (pidMap[i] != MAX_UINT_32) {
      for (uint8_t j = 0; j < BITS_IN_UINT32; j++) {
        if ((pidMap[i] & ((uint32_t)0x1 << j)) == 0) {
          pidMap[i] |= (uint32_t)0x1 << j;
          return i * BITS_IN_UINT32 + j;
        }
      }
    }
  }
}

uint32_t thread_free_pid(uint32_t pid) {
  uint32_t index = pid / BITS_IN_UINT32;
  uint8_t bitIndex = pid % BITS_IN_UINT32;
  pidMap[index] ^= (uint32_t)0x1 << bitIndex;
}

KernelStatus thread_default_suspend(struct Thread *thread) {
  // TODO:
  return OK;
}

KernelStatus thread_default_resume(struct Thread *thread) {
  // TODO:
  return OK;
}

KernelStatus thread_default_sleep(struct Thread *thread, uint32_t deadline) {
  // TODO:
  return OK;
}

KernelStatus thread_default_detach(struct Thread *thread) {
  // TODO:
  return OK;
}

KernelStatus thread_default_join(struct Thread *thread, int *returnCode, uint32_t deadline) {
  // TODO:
  return OK;
}

KernelStatus thread_default_exit(struct Thread *thread, uint32_t returnCode) {
  // TODO:
  return OK;
}

KernelStatus thread_default_kill(struct Thread *thread) {
  thread_free_pid(thread->pid);
  thread->stack->operations.free(thread->stack);
  // TODO:
  return OK;
}

uint32_t filestruct_default_openfile(FilesStruct *filesStruct, DirectoryEntry *directoryEntry) {
  FileDescriptor *fileDescriptor = (FileDescriptor *)kernelHeap.operations.alloc(&kernelHeap, sizeof(FileDescriptor));
  fileDescriptor->directoryEntry = directoryEntry;
  fileDescriptor->node.prev = nullptr;
  fileDescriptor->node.next = nullptr;
  fileDescriptor->pos = 0;

  KernelStatus status = kvector_add(filesStruct->fileDescriptorTable, &fileDescriptor->node);
  if (status != OK) {
    LogError("[Open]: file open failed, cause add fd table failed.\n");
    return 0;
  }
  return filesStruct->fileDescriptorTable->index - 1;
}

Thread *thread_default_copy(Thread *thread, CloneFlags cloneFlags, uint32_t heapStart) {
    Thread* p = thread_create(thread->name,thread->entry,thread->arg,thread->priority);
    if(cloneFlags & CLONE_VM){
        // TODO
    }

    if(cloneFlags & CLONE_FILES){
        // TODO
    }

    if(cloneFlags & CLONE_FS){
        // TODO
    }

    // TODO
    return p;
}

Thread *thread_create(const char *name, ThreadStartRoutine entry, void *arg, uint32_t priority) {
  // 1. allocate stack memory from kernel heap for idle task
  KernelStack *kernelStack = kstack_allocate(kernelStack);
  if (kernelStack != nullptr && kernelStack != nullptr) {
    // 1. init kernel stack
    kernelStack->operations.clear(kernelStack);

    kernelStack->operations.push(kernelStack, entry);      // R15 PC
    kernelStack->operations.push(kernelStack, entry);      // R14 LR
    kernelStack->operations.push(kernelStack, 0x12121212); // R12
    kernelStack->operations.push(kernelStack, 0x11111111); // R11
    kernelStack->operations.push(kernelStack, 0x10101010); // R10
    kernelStack->operations.push(kernelStack, 0x09090909); // R09
    kernelStack->operations.push(kernelStack, 0x08080808); // R08
    kernelStack->operations.push(kernelStack, 0x07070707); // R07
    kernelStack->operations.push(kernelStack, 0x06060606); // R06
    kernelStack->operations.push(kernelStack, 0x05050505); // R05
    kernelStack->operations.push(kernelStack, 0x04040404); // R04
    kernelStack->operations.push(kernelStack, 0x03030303); // R03
    kernelStack->operations.push(kernelStack, 0x02020202); // R02
    kernelStack->operations.push(kernelStack, 0x01010101); // R01
    kernelStack->operations.push(kernelStack, arg);        // R00
    kernelStack->operations.push(kernelStack, 0x600001d3); // cpsr

    Thread *thread = (Thread *)kernelHeap.operations.alloc(&kernelHeap, sizeof(Thread));
    thread->magic = THREAD_MAGIC;
    thread->threadStatus = THREAD_INITIAL;
    thread->stack = kernelStack;
    thread->priority = priority;
    thread->currCpu = INVALID_CPU;
    thread->lastCpu = INVALID_CPU;
    thread->entry = (ThreadStartRoutine)entry;

    thread->runtimeNs = 0;
    thread->runtimVirtualNs = 0;
    thread->startTime = ktimer_sys_runtime();

    thread->parentThread = nullptr;
    thread->pid = thread_alloc_pid();
    strcpy(thread->name, name);
    thread->arg = arg;

    thread->memoryStruct.vmmSpace.pageTableAddr = 0;
    thread->memoryStruct.vmmSpace.codeSectionAddr = 0;
    thread->memoryStruct.vmmSpace.roDataSectionAddr = 0;
    thread->memoryStruct.vmmSpace.dataSectionAddr = 0;
    thread->memoryStruct.vmmSpace.bssSectionAddr = 0;

    thread->threadList.prev = nullptr;
    thread->threadList.next = nullptr;

    thread->threadReadyQueue.prev = nullptr;
    thread->threadReadyQueue.next = nullptr;

    thread->rbNode.parent = nullptr;
    thread->rbNode.left = nullptr;
    thread->rbNode.right = nullptr;
    thread->rbNode.color = NODE_RED;

    thread->operations.suspend = thread_default_suspend;
    thread->operations.resume = thread_default_resume;
    thread->operations.sleep = thread_default_sleep;
    thread->operations.detach = thread_default_detach;
    thread->operations.join = thread_default_join;
    thread->operations.exit = thread_default_exit;
    thread->operations.kill = thread_default_kill;
    thread->operations.copy = thread_default_copy;

    thread->filesStruct.operations.openFile = filestruct_default_openfile;
    thread->filesStruct.fileDescriptorTable = kvector_allocate();

    LogInfo("[Thread]: thread '%s' created.\n", name);
    return thread;
  }
  LogError("[Thread]: thread '%s' created failed.\n", name);
  return nullptr;
}

uint32_t *idle_thread_routine(int arg) {
  while (1) {
    LogInfo("[Thread]: IDLE: %d \n", arg);
    asm volatile("wfi");
  }
}

Thread *thread_create_idle_thread(uint32_t cpuNum) {
  Thread *idleThread = thread_create("IDLE", idle_thread_routine, cpuNum, IDLE_PRIORITY);
  idleThread->cpuAffinity = cpuNum;
  // 2. idle thread
  idleThread->pid = 0;

  char idleNameStr[10] = {'\0'};
  strcpy(idleThread->name, itoa(cpuNum, &idleNameStr, 10));
  // TODO : other properties, like list
  LogInfo("[Thread]: Idle thread for CPU '%d' created.\n", cpuNum);
  return idleThread;
}

KernelStatus thread_reschedule() {
  // TODO
  return OK;
}
