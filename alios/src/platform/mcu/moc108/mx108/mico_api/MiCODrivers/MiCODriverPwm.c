#include "include.h"
#include "rtos_pub.h"
#include "MiCODriverPwm.h"

OSStatus MicoPwmInitialize(mico_pwm_t pwm, uint32_t frequency, float duty_cycle)
{
	return bk_pwm_initialize(pwm,frequency,duty_cycle);
}

OSStatus MicoPwmStart(mico_pwm_t pwm)
{
	return bk_pwm_start(pwm);
}

OSStatus MicoPwmStop(mico_pwm_t pwm)
{
	return bk_pwm_stop(pwm);
}

// eof

