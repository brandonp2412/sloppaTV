#pragma once

#include <ctime>

// libc may retain the zone used when the process first started; refresh it
// when Android changes the system zone or resumes the activity.
inline void refreshLocalClockTimeZone() { ::tzset(); }
