# voxl-docker-mavsdk-cpp

Example of how to use MAVSDK Python in a Docker container running on target.

## Summary

By following this example project, you will:

- Create a docker image on VOXL with [MAVSDK-Python](https://github.com/mavlink/MAVSDK-Python)
- Execute a **slightly modified** [takoff and land](https://github.com/mavlink/MAVSDK-Python/blob/master/examples/takeoff_and_land.py) program by utlizing [voxl-vision-px4](https://gitlab.com/voxl-public/voxl-vision-px4)
  - The only modification is is what parts are used by the program and mavsdk_server

## Prerequisites

- VOXL Flight or VOXL + Flight Core hardware
- For installation, the hardware needs to be connected to the internet, see [WiFi Station Mode configuration](https://docs.modalai.com/wifi-setup/#configure-station-mode) for more details
- [voxl-vision-px4](https://gitlab.com/voxl-public/voxl-vision-px4) v0.6.1 or newer needs to be configured using the [user guide](https://docs.modalai.com/voxl-vision-px4-installation/)
  - Note: we need to setup MAVLInk to route locally, edit `/etc/modalai/voxl-vision-px4.conf` file with the following:
    - v0.6.1 or newer:
    ```bash
    "en_localhost_mavlink_udp":	true,
    ```
    - older version:
    ```bash
    "en_secondary_qgc": true,
    "secondary_qgc_ip": "127.0.0.1",
    ```
- ADB is required for the build script to function properly, see [adb setup instructions](https://docs.modalai.com/setup-adb/) for details.

Note: VOXL ships with Docker, but requires a configuration script to run.  This will be handled in the `build-image.sh` script in this repository.  For more information, see the [Docker development user guide](https://docs.modalai.com/docker-on-voxl/) for more information.

## Pull Image from Registry

The docker image for this example is hosted in our registry and can be pulled onto VOXL.  Ensure VOXL is connected to the internet by setting  up VOXL into [Station Mode](https://docs.modalai.com/wifi-setup/#configure-station-mode).  Once connceted to the internet, proceed with the following:

```bash
adb shell
```

Now on VOXL, enable docker support:

```bash
voxl-configure-docker-support.sh
```

Pull the image:

```bash
docker pull gcr.io/modalai-public/voxl-mavsdk-python:v1.1
```

*Note: If you'd like to build the image yourself, there are instructions at the end of this README.*

## Execute the Example Program

### Overview

After the docker image is build following the steps above, we can SSH onto the vehicle over WiFi and then execute the example program from the container.  The program in the container communicates using UDP to voxl-vision-px4 and then over UART to PX4 running on the flight controller (the STM32 on VOXL Flight or Flight Core).

### Usage

#### Warning

This example instructs the vehicle to takeoff to an altitude of roughly 2.5m, hold for several seconds, and then land.

#### Connect to Network

We have alraedy setup VOXL in [Station Mode](https://docs.modalai.com/wifi-setup/#configure-station-mode) when we downloaded the image.  

Run `ifconfig` to get the IP address of VOXL.  The following will assume the VOXL has an IP address of `192.168.1.100`.

- SSH onto VOXL using the following:

```bash
me@mylaptop:~$ ssh root@192.1681.100
(password: oelinux123)
```

- Open the terminal in the container using the `docker run` command:

  *Note: if you built the image locally, you need to use the image name you build with, e.g. `voxl-mavsdk-python:v1.1`*

```bash
docker run -it --rm --privileged --net=host gcr.io/modalai-public/voxl-mavsdk-python:v1.1 /bin/bash
```

- We need to start mavsdk_server in the **background**
  - Note: for voxl-vision-px4 0.6.1 or newer, user port 14551.  For older versions, use port 14550.

```bash
./start-mavsdk-server.sh

[Hit ENTER to get shell access]
```

- Run the example `takeoff_and_land2.py`:
  - Note: for voxl-vision-px4 0.6.1 or newer, user port 14551.  For older versions, use port 14550.


```bash
python3 takeoff_and_land2.py udp://:14551
```

Here's output from a test flight from the previous command:

```bash
root@apq8096:/home/MAVSDK-Python/examples# python3 takeoff_and_land2.py udp://:14551
Waiting for mavsdk_server to be ready...
Connected to mavsdk_server!
Waiting for drone to connect...
Drone discovered with UUID: 3546673866589352499
Waiting for drone to have a global position estimate...
Global position estimate ok
-- Arming
[05:03:02|Debug] MAVLink: info: ARMED by Arm/Disarm component command (system_impl.cpp:257)
-- Taking off
[05:03:02|Debug] MAVLink: info: [logger] /fs/microsd/log/2020-07-10/17_03_02.ulg (system_impl.cpp:257)
[05:03:02|Debug] MAVLink: info: Using minimum takeoff altitude: 2.50 m (system_impl.cpp:257)
[05:03:04|Debug] MAVLink: info: Takeoff detected (system_impl.cpp:257)
-- Landing
[05:03:07|Debug] MAVLink: info: Landing at current position (system_impl.cpp:257)
root@apq8096:/home/MAVSDK-Python/examples# [05:03:14|Debug] MAVLink: info: Landing detected (system_impl.cpp:257)
[05:03:16|Debug] MAVLink: info: DISARMED by Auto disarm initiated (system_impl.cpp:257)
```

## Optional: Building the Image

To build the docker image with MAVSDK-Python and the example program:

- connect VOXL Flight or VOXL + Flight Core hardware to host machine using USB
- run the build script:

```bash
./build-image.sh
```

Note: this is a LONG process, it takes **roughly 80 minutes**, in large part due to the `grpcio` package that needs to be built for armv8, and concludes with a message like:

```bash
Step 20 : COPY takeoff_and_land2.py .
 ---> 2f5a4181cbc8
Removing intermediate container 7baafa6060a4
Step 21 : CMD /bin/bash
 ---> Running in ab8fad26d032
 ---> 98d4b52a5860
Removing intermediate container ab8fad26d032
Successfully built 98d4b52a5860bf0226e7c
Successfully built ec9462df4305
```
