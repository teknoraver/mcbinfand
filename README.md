# MACCHIATObin Fan daemon

The [MACCHIATObin](http://macchiatobin.net/) board has a PWM fan connected to the J10 header.  
By default the fan spin at maximum speed, but it can be switched on/off via GPIO #80 (precisely GPIO #16 of the second chip).  

## mcbinfand
`mcbinfand` is a small daemon which reads the temperature from the _armada-cp110-thermal"_ hwmon iterface, and sets the fan speed via a software PWM.
The fan duty cycle is set at 0% with temperature below 40 °C and 100% when temperature goes above 55 °C.
For temperatures in the middle, duty cycle is scaled proportionally.

## Compiling
`mcbinfand` only depends on [libgpiod](https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/)

## Running
`mcbinfand` is not configurable at runtime yet, if you want to tune it, set these variables in `fand.c`:
* MIN_TEMP the min temperature, e.g. the one with 0% duty cycle
* MAX_TEMP the max temperature, e.g. with 100% duty cycle
* PWM_USECS the interval in microseconds of the PWM loop. Lower values will keep the CPU busy

On exit the daemon will leave the GPIO on, so the fan will spin at maximum speed and your CPU won't burn.  
`fand.service` is a systemd unit file which starts the daemon just after the creation of the `/dev/gpiochip1` character device.
