/* Unmodified algorithm from JoyShockMapper 980dc52c, main.cpp.
 * Copyright 2018-2021 Julian "Jibb" Smart and contributors. MIT.
 * Mechanical C substitutions: state prefix, fabsf, capacity literal. */
	static float jsm_reference_smooth(struct NativeFlick *f, float value, float bottomThreshold, float topThreshold, int maxSamples)
	{
		// which sample in the circular smoothing buffer do we want to write over?
		f->front_sample--;
		if (f->front_sample < 0)
			f->front_sample = 64 - 1;
		// if this input is bigger than the top threshold, it'll all be consumed immediately; 0 gets put into the smoothing buffer. If it's below the bottomThreshold, it'll all be put in the smoothing buffer
		float length = fabsf(value);
		float immediateFactor;
		if (topThreshold <= bottomThreshold)
		{
			immediateFactor = 1.0f;
		}
		else
		{
			immediateFactor = (length - bottomThreshold) / (topThreshold - bottomThreshold);
		}
		// clamp to [0, 1] range
		if (immediateFactor < 0.0f)
		{
			immediateFactor = 0.0f;
		}
		else if (immediateFactor > 1.0f)
		{
			immediateFactor = 1.0f;
		}
		float smoothFactor = 1.0f - immediateFactor;
		// now we can push the smooth sample (or as much of it as we want smoothed)
		float frontSample = f->samples[f->front_sample] = value * smoothFactor;
		// and now calculate smoothed result
		float result = frontSample / maxSamples;
		for (int i = 1; i < maxSamples; i++)
		{
			int rotatedIndex = (f->front_sample + i) % 64;
			frontSample = f->samples[rotatedIndex];
			result += frontSample / maxSamples;
		}
		// finally, add immediate portion
		return result + value * immediateFactor;
	}
