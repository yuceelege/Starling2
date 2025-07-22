# voxl-tflite-server

Use tensorflow lite on VOXL for various deep learning tasks

dependencies:
* libmodal-pipe
* libmodal-json
* voxl-opencv
* voxl-tflite
    * apq8096-tflite OR qrb5165-tflite

## Build Environment

This project builds in the voxl-cross docker image (>= V1.7)

Follow the instructions here to build and install the voxl-cross docker image:
https://gitlab.com/voxl-public/voxl-docker

## Build Instructions

1) Launch the voxl-cross docker.

```bash
~/git/voxl-tflite-server# voxl-docker -i voxl-cross
bash-4.3$ ls
README.md         clean.sh  include                ipk              service
bash_completions  config    install_build_deps.sh  make_package.sh
build.sh          dnn       install_on_voxl.sh     server
```


2) Install dependencies inside the docker. You must specify both the hardware platform and binary repo section to pull from. CI will use the `dev` binary repo for `dev` branch jobs, otherwise it will select the correct target SDK-release based on tags. When building yourself, you must decide what your intended target is, usually `dev` or `staging`

```bash
voxl-cross:~$ ./install_build_deps.sh qrb5165 dev
```


3) Build scripts should take the hardware platform as an argument: `qrb5165` or `apq8096`. CI will pass these arguments to the build script based on the job target. It's recommended your build script also allow a `native` option that uses the native GCC instead of a cross-compiler. This is handy for local building and testing of hard-ware independent code.

The build script in this template will build for 64-bit using different cross compilers for each hardware platform. See libmodal-pipe for an exmaple of building for both 64 and 32 bit.

```bash
voxl-cross:~$ ./build.sh qrb5165
OR
voxl-cross:~$ ./build.sh apq8096
```


4) Make an ipk or deb package while still inside the docker.

```bash
voxl-cross:~$ ./make_package.sh deb
OR
voxl-cross:~$ ./make_package.sh ipk
```

This will make a new package file in your working directory. The name and version number came from the package control file. If you are updating the package version, edit it there.

Optionally add the --timestamp argument to append the current data and time to the package version number in the debian package. This is done automatically by the CI package builder for development and nightly builds, however you can use it yourself if you like.


## Deploy to VOXL

You can now push the ipk or deb package to VOXL and install with dpkg/opkg however you like. To do this over ADB, you may use the included helper script: deploy_to_voxl.sh. Do this outside of docker as your docker image probably doesn't have usb permissions for ADB.

The deploy_to_voxl.sh script will query VOXL over adb to see if it has dpkg installed. If it does then then .deb package will be pushed an installed. Otherwise the .ipk package will be installed with opkg.

```bash
(outside of docker)
voxl-tflite-server$ ./deploy_to_voxl.sh
```

This deploy script can also push over a network given sshpass is installed and the VOXL uses the default root password.


```bash
(outside of docker)
voxl-tflite-server$ ./deploy_to_voxl.sh ssh 192.168.1.123
```

# Implementing your Model in voxl-tflite-server

Integrating your models into the voxl-tflite-server is quite easy using the generic base classes provided as a template to allow your model to work seamlessly with the voxl hardware. 

## Set up

To get started, you must first move your __.tflite__ file to `/usr/bin/dnn/` on your voxl. You would also neded to do the same for your labels file.

In the function `initialize_model_settings` in the `main` file, add a conditional corresponding to your particular model in the same format as the others. Models in the voxl-tflite-server are indexed using two attributes, the model name and the model category. For example, the objection detection yolov8 model has the model name `YOLOV8` and the model category `OBJECT_DETECTION`.

Then in the `model_helper.cpp`, in the function `create_model_helper`, add a switch statement corresponding to your attributes and return an instantiation of your model class (more details on that in the next section)

## Model class

You need to create a header and source file for your model and write the corresponding Helper class. Your class must inherit the common base class provided in __model_helper.h__ and __model_helper.cpp__. 

Your class will have the following methods

* __preprocess__ : This method contains the preprocesing logic for your model before it is passed for inference. There is a default implementation provided in the model_helper.cpp. It can be overriden if needed. 

* __run_inference__: This method invokes the tflite interpreter using your `.tflite` file and passes in the preprocessed input. It can be overriden if your model requires different logic.

* __post_process__: This method needs to be implemented by you according to your model's requirements. This method is invoked by the __worker__ method.

* __worker__: This method performs operations external to the model which are required by other voxl functions such as passing postprocessed images to their relevant pipes and timestamping frames. This method invokes the postprocess method and must also be implemented based on model functionality. You may have a look at the other worker methods to get an idea on how to write your own.

By following the above steps, you will be able to add any custom model to your voxl with complete freedom to create your own pre-processing, inferencing, and post-processing logic. 








