/*
 * Copyright (C) 2022 Billy Laws
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ST_HWSYSFSENSOR_BASE_H
#define ST_HWSYSFSENSOR_BASE_H

#include <poll.h>
#include <math.h>

#include "HWSensorBase.h"

extern "C" {
	#include "utils.h"
};

/*
 * class Light
 */
class Light : public SensorBase {
private:
	HWSensorBaseCommonData common_data;
	volatile int64_t poll_period_ns;

public:
	Light(HWSensorBaseCommonData *data,
		  const char *name, int handle,
		  float power_consumption);

	virtual int Enable(int handle, bool enable, bool lock_en_mute);
	virtual int SetDelay(int handle, int64_t period_ns, int64_t timeout, bool lock_en_mute);

	virtual void ThreadDataTask();

	bool hasDataChannels() { return true; }
};

#endif /* ST_HWSYSFSSENSOR_BASE_H */
