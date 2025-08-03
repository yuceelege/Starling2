FROM arm64v8/ubuntu:focal

WORKDIR /home

RUN apt-get update
RUN apt-get install -y apt-transport-https ca-certificates gnupg software-properties-common wget
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | apt-key add -
RUN apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main'
RUN apt-get update
RUN apt-get install -y cmake build-essential colordiff git doxygen -y
RUN apt-get install -y python3 python3-pip -y 
RUN apt-get install -y git libxml2-dev libxslt-dev curl



RUN git clone https://github.com/mavlink/MAVSDK.git

WORKDIR /home/MAVSDK
RUN git checkout main
RUN git submodule update --init --recursive

RUN cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_MAVSDK_SERVER=ON -DBUILD_SHARED_LIBS=ON -Bbuild/default -H.
RUN cmake --build build/default --target install -- -j 4
RUN ldconfig

# Copy all three ModalAI .deb packages into the image
COPY libvoxl-cutils_0.1.5_arm64.deb libmodal-json_0.4.7_arm64.deb libmodal-pipe_2.13.2_arm64.deb /tmp/

# Install them (ignore missing-deps errors), then pull in any dependencies, then clean up
RUN dpkg -i /tmp/libvoxl-cutils_0.1.5_arm64.deb \
             /tmp/libmodal-json_0.4.7_arm64.deb \
             /tmp/libmodal-pipe_2.13.2_arm64.deb || true
#RUN apt-get update
RUN apt-get install -f -y
RUN rm /tmp/libvoxl-cutils_0.1.5_arm64.deb \
        /tmp/libmodal-json_0.4.7_arm64.deb \
        /tmp/libmodal-pipe_2.13.2_arm64.deb

# Force symlinks to the 64-bit versions
RUN ln -sf /usr/lib64/libmodal_pipe.so /usr/lib/libmodal_pipe.so && \
    ln -sf /usr/lib64/libmodal_json.so /usr/lib/libmodal_json.so && \
    ln -sf /usr/lib64/libmodal_exposure.so /usr/lib/libmodal_exposure.so || true && \
    ln -sf /usr/lib64/libvoxl_common_config.so /usr/lib/libvoxl_common_config.so || true

WORKDIR /home/MAVSDK/examples/takeoff_and_land
RUN cmake .
RUN make

WORKDIR /home

RUN python3 -m pip install -U pip
RUN python3 -m pip install asyncio
RUN python3 -m pip install math
RUN git clone https://github.com/mavlink/MAVSDK-Python.git /home/MAVSDK-Python
WORKDIR /home/MAVSDK-Python
RUN git checkout tags/2.8.0 -b 2.8.0 \
 && git submodule update --init --recursive \
 && pip3 install .


# RUN echo ">>> DEBUG LIB INFO:" && \
#     ls -l /usr/lib/libmodal_pipe.so && \
#     file /usr/lib/libmodal_pipe.so && \
#     readelf -h /usr/lib/libmodal_pipe.so || true && \
#     exit 1

COPY mpa_reader.c /home/mpa_reader.c

# Copy the proprietary qti-gbm package
COPY qti-gbm_18.0.0-rc5_arm64.deb /tmp/

# Install it
RUN dpkg -i /tmp/qti-gbm_18.0.0-rc5_arm64.deb \
 && rm /tmp/qti-gbm_18.0.0-rc5_arm64.deb
RUN apt-get update && apt-get install -y libwayland-server0

RUN gcc -o /home/mpa_reader /home/mpa_reader.c -lmodal_pipe
RUN gcc -o /home/mpa_writer /home/mpa_writer.c -lmodal_pipe

COPY neural_control.py /home/neural_control.py

WORKDIR /home
COPY voxl2_takeoff_land.py check_status.py vicon_test.py offboard_8_figure.py offboard_attitude.py offboard_position_velocity_ned.py offboard_velocity_body.py offboard_velocity_ned.py start-mavsdk-server.sh run-docker.sh .
COPY mpa_pipe_reader.py /home/mpa_pipe_reader.py
RUN chmod +x start-mavsdk-server.sh
RUN chmod +x run-docker.sh
# RUN cp /home/MAVSDK/examples/takeoff_and_land/takeoff_and_land ./voxl2_takeoff_land

CMD ["/bin/bash"]
