# General Information

The upcoming information in this part is a distilled version of [https://docs.modalai.com/](https://docs.modalai.com/) of parts important for us.

## 1. Set up ADB
[https://docs.modalai.com/setting-up-adb/](https://docs.modalai.com/setting-up-adb/) . Instructions are the same and definitely dont skip this part in the link: **"ModalAI Top Tip: Aliases"**. It allows you to directly connect to VOXL by typing "voxl" on the terminal given USB is connected.

**ADB is very helpful** for establishing a terminal connection and doing file push/pull directly through USB, without SSH. 

## 2. Wifi Configuration
(source: [https://docs.modalai.com/voxl-2-wifi-setup/](https://docs.modalai.com/voxl-2-wifi-setup/))

1. write "voxl" in the host terminal to ADB into your device
2. the command "voxl-wifi wizard" gives you "station", "softap", "factory" options

- **station (2)** : allows you to connect to a wifi network using SSID and password
- **softap (3)** : VOXL 2 creates its own wifi hotspot with the password 123456789
- **factory (4)** : resets to factory mode, which is softap.

After connecting to wifi, you can disconnect adb and directly connect through SSH if you wish. (passwd: oelinux123)
 

## 3. Modal Pipe Architecture / Basic Tools and Utilities

VOXL 2 has an internal structure similar to ROS. There are many services like camera, imu, tflite, mapper, etc... each running its internal process asynchronously. **Modal Pipe Architecture (MPA)** establishes pipes between these services and communicates the necessary information across various parts of the drone to maintain vital functionalities e.g. flight, navigation, telemetry etc.

- [https://docs.modalai.com/mpa/](https://docs.modalai.com/mpa/) tells you how you can view these services and stop/start/enable/disable/restart them through systemctl.
- [https://docs.modalai.com/basic-functionalities/](https://docs.modalai.com/basic-functionalities/) gives further information on how you can view the information flowing from some of the pipes e.g.: imu, battery,cpu. (Useful for viewing whether CPU is overheated or how much battery left)


## 4a. Installing Q Ground Control (QGC):

This step is straightforward. Install [https://docs.modalai.com/installing-qgc/](https://docs.modalai.com/installing-qgc/) 

## 4b. Connecting to QGC:
(source: [https://docs.modalai.com/qgc-wifi/](https://docs.modalai.com/qgc-wifi/))

Drone automatically connects to QGC in the softap mode (drone hotspot mode). For establishing connection in the station mode, write the following in the voxl terminal: `nano /etc/modalai/voxl-mavlink-server.conf`

Drone was assigned ""192.168.8.10" in the softap mode by default that is why QGC connects automatically in that mode since "192.168.8.10" is also written in voxl-mavlink-server.conf. Change this IP address to the right address by copying the ip from "ifconfig" on the voxl machine. 


## 5. First flight
Check the video in this link for a safe first flight. In there, important features like, kill switch, safe/position mode change, RC controls are explained: [https://docs.modalai.com/starling-first-flight/](https://docs.modalai.com/starling-first-flight/) 

## 6. Installing voxl-docker / voxl-cross (Important Step)

Whenever we need to write custom C++ code directly inside the VOXL 2 machine, we need to build the code with all the dependencies present in the VOXL 2 machine. **voxl-cross** is a docker container, having all the necessary packages for our drone software and contains arm 32 and arm 64 compiler necessary for C++ builds. **voxl-docker** is a bash script that launches this docker script.

**Step 1**: Install the voxl-docker from here [https://docs.modalai.com/voxl-docker-and-cross-installation/](https://docs.modalai.com/voxl-docker-and-cross-installation/) . Installations are done on the host computer, not on the drone. Proceed only until "Installing voxl-cross" step. **DO NOT INSTALL voxl-cross_V2.5**. The reason will be explained later. For now, please install voxl-cross V2.7 from [https://developer.modalai.com/asset/5](https://developer.modalai.com/asset/5) . You may need to first open an account in modalAI website to access the file, along with any other developer source code. We will install voxl-cross v2.7 from here.

Once you obtain voxl-cross_v2.7.tgz , proceed the voxl-cross installation with this file e.g. 

docker load -i voxl-cross_v2.7.tgz <br>
docker tag voxl-cross:V2.7 voxl-cross:latest <br>
voxl-docker -i voxl-cross


Lastly check out this link [https://docs.modalai.com/voxl-cross-template/](https://docs.modalai.com/voxl-cross-template/) to see the structure of voxl-cross container, which willl be important later.

**Why voxl-cross / voxl-docker is important for us?**: This is future reference but in order to run custom neural networks, we will need to use a modified clone of "[https://gitlab.com/voxl-public/voxl-sdk/services/voxl-tflite-server](https://gitlab.com/voxl-public/voxl-sdk/services/voxl-tflite-server)" (already in repo) and change the C++ code to make the input output tensors compatible with our network and that requires writing C++ code which is compiled by voxl-cross. We then create a .deb file and deploy this to the voxl. 

**voxl-tflite-server** is an MPA (introduced in number 3.) that publishes the output results of the neural network we choose.

—-----------------------------------------------------------------------------------------------------

# Understanding VIO

—-----------------------------------------------------------------------------------------------------

# Running Python Scripts in VOXL

We use **MAVSDK** to communicate our python script with the PX4 autopilot through Mavlink.
We need a docker container to run MAVSDK-Python as Voxl lacks some compilers necessary for running python code.

## Docker Installation:
Normally we clone the ascend/docker branch of this repo: [https://gitlab.com/voxl-public/voxl-docker-images/voxl-docker-mavsdk-python](https://gitlab.com/voxl-public/voxl-docker-images/voxl-docker-mavsdk-python) 

**BUT**, there were some issues with the most recent MAVSDK version we were previously seeing an error similar to this forum message [https://forum.modalai.com/topic/3463/voxl-docker-mavsdk-python-build-issues](https://forum.modalai.com/topic/3463/voxl-docker-mavsdk-python-build-issues) . I fixed these issues so just install the modified repo from here.

## Docker Installation Steps:

1. Connect the drone with USB, also connect the drone to a wifi network.
2. Enter the voxl-docker-mavsdk-python directory you just installed and run `./build-image.sh` (This will push the important files to voxl and build the docker inside the voxl so keep USB connected always in the process!)
3. When build completes, enter the voxl machine by typing "voxl" on another command line. Go into the directory: `/data/docker/mavsdk-python`
4. run `./run-docker.sh`
5. Now you are inside MAVSDK docker. Run `./start-mavsdk-server` to initialize mavlink connection with the autopilot.
6. Lastly, you can execute any of the below python code to achieve certain navigation functionalities with the aid of VIO. 

(VIO is activated by default, we will disable it when state based control is required.)

**Note**: Make sure you apply the safety procedures at this point before running any Python script (get familiar with kill switch /manual, position, offboard mode) for a possible intervention if things go wrong.

## Scripts

**voxl2_takeoff_land.py**: Performs stable take off and elevates for a certain amount of time and then performs stable landing.

You can directly execute this code just place the drone at the middle of the arena and run the script. But be prepared to switch to manual mode to take over control or kill switch if things go wrong.

The following are specific scripts which allow different ways of controlling the drone offboard_attitude_rate.py is the lowest level of control where we can set trust and 3 orientation yaw rates (similar to 12d drone dynamics)

| Script                      | Setpoint Method         | Control Mode                          |
|-----------------------------|-------------------------|---------------------------------------|
| offboard_position_ned.py    | set_position_ned()      | Position + yaw in NED frame           |
| offboard_velocity_ned.py    | set_velocity_ned()      | NED velocity + absolute yaw angle     |
| offboard_velocity_body.py   | set_velocity_body()     | Body-relative velocity + yaw rate     |
| offboard_attitude.py        | set_attitude()          | Absolute attitude angles + thrust     |
| offboard_attitude_rate.py   | set_attitude_rate()     | Body rate control + thrust (4D control) |

The templates for offboard_* scripts are present in MAVSDK docker and will only require simple parameter changes. 

If you are curious, you can check out more examples from [https://github.com/mavlink/MAVSDK-Python/tree/main/examples](https://github.com/mavlink/MAVSDK-Python/tree/main/examples) but don't directly use them. Do the necessary changes by comparing with the code I provided you. I bypass some telemetry checks as we do not have gps so be careful with how I implemented the other codes. 

**Note**: A safe way to use offboard_* scripts would be to first arm the drone and hover it at some level using either the manual or the position mode. Then when you are ready, switch to the offboard mode and execute the python script and be prepared to take over if anything goes wrong.

—--------------------------------------------------------------------------------------------------------------

# Custom Neural Network Deployment

You need to modify voxl-tflite-server repository for your specific neural network model to deploy custom neural networks e.g. adjust the input output tensors to be compatible with your target model. Let your custom model be called as custom_network without loss of generality. Here are the steps to add your own model:

---

## ➤ Step 1: Create Header File

Go /include/model_helper directory. Create custom_network_helper.h header. See the example. gate_xyz_model_helper.h I already made for another network that outputs xyz pose of gates.

This header file defines the GateXyzModelHelper class, a specialized helper for running inference with a specific neural network model that estimates gate positions in 3D. It extends ModelHelper and customizes the worker and postprocess functions for model-specific behavior during inference.

These functions don’t really change except helper and class names unless you want to do some postprocessing on inference outputs before publishing on the mpa pipe.

---

## ➤ Step 2: Create Source File

Go /src/model_helper directory. Create custom_network_helper.cpp . See the example. gate_xyz_model_helper.cpp in the same directory

This source file implements the GateXyzModelHelper class defined in the header. It runs inference using a neural network model and writes the predicted 3D gate position (x, y, z) with a timestamp to a pipe. The worker() method extracts the model output, formats it into a GateXyzMsg, prints it, and sends it through a pipe for downstream use. The postprocess() method is a stub and does nothing.

You can change the format of output that is written on the pipe in this code for your custom application. Since we didn’t need post processing, we left it empty.

---

## ➤ Step 3: Register Model Name

Go to /include/model_helper/model_info.h and define your custom Model Name under enum ModelName{} along with other names.

---

## ➤ Step 4: Update main.cpp

Go to main.cpp at the top directory. Add an “else if” condition at the very end to add your custom model in the same directory (this directory is inside VOXL 2 and we will eventually push our model into this directory through ADB). Write the model name you defined in model_info.h at model_name section, you can write OBJECT_DETECTION at the model_category section as this is not very important. Lastly, you can keep norm_type = None

---

## ➤ Step 5: Build the Code


voxl-docker -i voxl-cross<br>
./install_build_deps.sh qrb5165 sdk-1.3 to install the dependencies<br>
./clean.sh<br>
./build.sh qrb5165



Note: This is why I previously urged to install voxl-cross 2.7. According to we were previously facing build errors due to some bug so we are following the solution here https://forum.modalai.com/topic/4569/tensorflow-lite-dev-setup-failing-on-ubuntu-22 to avoid this.

## ➤ Step 6: Deploy the Package

When the build is complete, run

voxl-cross:~$ ./make_package.sh deb

This will create a deb file which will be installed inside the VOXL 2. Now leave the docker and run this last command to push the deb file inside the VOXL 2 (make sure USB connected) and install it automatically

voxl-tflite-server$ ./deploy_to_voxl.sh

