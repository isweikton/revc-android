#pragma once

#include <cstdio>

extern FILE* logfile;

void AndroidSetLoadingOverlay(const char *title, const char *subtitle, bool visible);
void AndroidShowSettingsMenu();
