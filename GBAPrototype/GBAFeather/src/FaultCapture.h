#pragma once

// Reserve scratch[4] for Pico SDK watchdog/reboot magic. The fault record
// uses existing application scratch[2..3] and spare scratch[5..7].
void gbaInstallFaultCapture();
