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

## NixOS setup
On NixOs you can compile the binary using packages [gcc](https://search.nixos.org/packages?channel=23.11&show=gcc&from=0&size=50&sort=relevance&type=packages&query=gcc) and [libgpiod_1](https://search.nixos.org/packages?channel=23.11&show=libgpiod_1&from=0&size=50&sort=relevance&type=packages&query=libgpio). You can do this by adding the packages to your system packages in your configuration.nix:

```
  environment.systemPackages = with pkgs; [
    #...your other packages go here
    #to build the fan control daemon
    gcc
    libgpiod_1 
  ];  
```

or in a nix shell

```
nix-shell -p gcc libgpiod_1
```

You don't need the packages installed to run the service once the binary is built.

once you have the binary, you can install it as a systemd service as follows:

```
  systemd.services.mcbinfand = {
    enable = true;
    description = "Controls fan speed";
    serviceConfig = {
       ExecStart="./your_path_goes_here/fand"; # this can be an absolute path on your target machine
       # ExecStart="${./fan_control/fand}"; or a relative path to the file that declares it
       Restart = "always";
    };
    wantedBy = ["multi-user.target"];
    after = ["dev-gpiochip0.device"];
  };
```
