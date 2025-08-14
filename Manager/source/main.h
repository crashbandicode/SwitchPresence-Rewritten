#pragma once

#include <switch.h>
#include <vector>
#include <string>

#include "states/statemachine.h"

#include "states/MainMenu.h"
#include "states/ErrorState.h"
#include "states/DumpResState.h"
#include "states/DumpCompleteState.h"

// Declare the global pad object
extern PadState pad;