#include "main.h"

// Define the global pad object
PadState pad;

int main(int argc, char **argv)
{
    consoleInit(nullptr);

    // Initialize the pad state to listen to any controller
    padInitializeDefault(&pad);

    states::StateMachine stateMachine;

    stateMachine.states.push_back(new states::MainMenu());
    stateMachine.states.push_back(new states::DumpResState());
    stateMachine.states.push_back(new states::DumpCompleteState());
    stateMachine.states.push_back(new states::ErrorState());

    if (R_FAILED(states::ErrorState::error))
        stateMachine.pushState("error");
    else
        stateMachine.pushState("main");

    while (appletMainLoop())
    {
        // Scan the controller state
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad); // Get the buttons that were just pressed
        if (kDown & HidNpadButton_Plus || (kDown & HidNpadButton_B && stateMachine.currentState->name() == "main"))
            break;

        printf(CONSOLE_ESC(2J));
        stateMachine.calc(kDown);
        consoleUpdate(nullptr);
    }

    consoleExit(nullptr);
    return 0;
}

extern "C"
{
    void userAppInit(void)
    {
        Result rc;
        rc = pmshellInitialize();
        if (R_FAILED(rc))
            goto error;
        rc = pmdmntInitialize();
        if (R_FAILED(rc))
            goto error;
        rc = nsInitialize();

        error:
            states::ErrorState::error = rc;
    }

    void userAppExit(void)
    {
        nsExit();
        pmdmntExit();
        pmshellExit();
    }
}