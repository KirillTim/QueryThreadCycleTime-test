#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>

#include <Windows.h>
#include <process.h>

using namespace std;

void waitingFunction(void* ignored) {
    cerr << "waitingFunction is working on thread " << GetCurrentThreadId() << endl;
    this_thread::sleep_for(chrono::seconds(1000));
}

enum class ThrState {
    Unknown, Running, Sleeping
};

bool is_thread_waiting(DWORD64 instruction_pointer) {
    // 0f 05 (syscall)
    // c3    (ret)   <- instruction pointer
    unsigned char code[] = {0x0f, 0x05, 0xc3};
    // Note: IP can be the first byte of readable memory. e.g when it can be the first shell code instruction
    return *((unsigned char *) instruction_pointer) == code[2] &&
           *((unsigned char *) (instruction_pointer - 1)) == code[1] &&
           *((unsigned char *) (instruction_pointer - 2)) == code[0];
}

void sampler(void* ignored) {
    int target_tid;
    cout << "Enter target tid" << endl;
    cin >> target_tid;
    const int flags = THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT |
                      THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION;
    HANDLE threadHandle = OpenThread(flags, false, target_tid);
    if (threadHandle == nullptr) {
        cerr << "threadHandle == nullptr" << endl;
        return;
    }

    DWORD64 lastRip = 0;
    ThrState lastState = ThrState::Unknown;
    ULONG64 lastCycleTime = 0;

    FILETIME last_kernelTime;
    FILETIME last_userTime;

    for (;;) {
        CONTEXT ctxt;
        ctxt.ContextFlags = CONTEXT_FULL;
        DWORD suspend_count = SuspendThread(threadHandle);
        if (suspend_count != 0) {
            cerr << "can't SuspendThread" << endl;
            break;
        }
        if (GetThreadContext(threadHandle, &ctxt) == 0) {
            cerr << "can't GetThreadContext" << endl;
            break;
        }
        ULONG64 cycle_time;
        if (QueryThreadCycleTime(threadHandle, &cycle_time) == 0) {
            cerr << "can't QueryThreadCycleTime, error code: " << GetLastError() << endl;
            break;
        }
        ThrState state = is_thread_waiting(ctxt.Rip) ? ThrState::Sleeping : ThrState::Running;
        if (lastState == ThrState::Sleeping) {
            if (lastCycleTime != cycle_time) {
                cerr << "lastCycleTime != cycle_time: " << "lastCycleTime: " << lastCycleTime << ", cycle_time: " << cycle_time << endl;
            } else {
                cerr << "lastCycleTime == cycle_time" << endl;
            }
            cerr << "lastRip:" << lastRip << ", current Rip: " << ctxt.Rip << endl;
        } else if (lastState == ThrState::Running) {
            cerr << "lastState == ThrState::Running" << endl;
        }

        FILETIME creationTime;
        FILETIME exitTime;
        FILETIME kernelTime;
        FILETIME userTime;

        if (GetThreadTimes(threadHandle, &creationTime, &exitTime, &kernelTime, &userTime) == 0) {
            cerr << "can't GetThreadTimes, error code: " << GetLastError() << endl;
            break;
        }

        if (lastState == ThrState::Sleeping) {
            if (userTime.dwHighDateTime == last_userTime.dwHighDateTime && userTime.dwLowDateTime == last_userTime.dwLowDateTime) {
                cerr << "userTime == last_userTime" << endl;
            } else {
                cerr << "userTime != last_userTime" << endl;
            }
            if (kernelTime.dwHighDateTime == last_kernelTime.dwHighDateTime && kernelTime.dwLowDateTime == last_kernelTime.dwLowDateTime) {
                cerr << "kernelTime == last_kernelTime" << endl;
            } else {
                cerr << "kernelTime != last_kernelTime" << endl;
            }
        }

        last_kernelTime = kernelTime;
        last_userTime = userTime;

        lastRip = ctxt.Rip;
        lastState = state;
        lastCycleTime = cycle_time;

        ResumeThread(threadHandle);
        this_thread::sleep_for(chrono::milliseconds(17));
    }
}


int main() {
    _beginthread(waitingFunction, 0, nullptr);
    _beginthread(sampler, 0, nullptr);
    cerr << "Waiting..." << endl;
    for (;;) {

    }
    return 0;
}