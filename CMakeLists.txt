#
# For compilation, the following environment vars should be defined before running cmake:
# 	USERLANDPATH	- path to the root of the Pi userland files (from https://github.com/raspberrypi/userland.git)
#
# For cross-compilation, the following environment vars should be defined before running cmake:
# 	ROOTFSPATH		- path to the root of the Pi file system (for include and library file paths)
#
# Optional environment vars:
#	DEBUG		- if set to any value, will build debug version (no optimisation, full symbol information).
#

project(motion)
cmake_minimum_required(VERSION 2.8)

set(ROOTFSPATH $ENV{ROOTFSPATH})
set(USERLANDPATH $ENV{USERLANDPATH})

if(NOT USERLANDPATH)
	set(USERLANDPATH /opt/vc)
endif(NOT USERLANDPATH)

execute_process(COMMAND uname -m OUTPUT_VARIABLE MACHINE)

enable_language(ASM)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(
					${USERLANDPATH}/include
					${USERLANDPATH}/include/interface/vcos
					${USERLANDPATH}/include/interface/vcos/pthreads
					${USERLANDPATH}/include/interface/vmcs_host/linux
					${USERLANDPATH}/include/interface/vmcs_host
					)

add_definitions(	-DVERSION="mmaltest"
					-Dsysconfdir="/etc"
					-D_REENTRANT
					-DHAVE_OMX
					-DMOTION_V4L2
					-DMOTION_V4L2_OLD
					-DTYPE_32BIT=int
					-DHAVE_BSWAP
					-Wall
					-fmessage-length=0
					-std=gnu99
					)

if ($ENV{DEBUG})
	add_definitions(-O0 -g3 -D_DEBUG)
else()
	add_definitions(-O3)
endif()

link_directories(
				${ROOTFSPATH}/opt/vc/lib
				)

add_executable(motion
				alg.c
				alg_arm.s
				conf.c
				draw.c
				event.c
				ffmpeg.c
				filecam.c
				jpegutils.c
				logger.c
				md5.c
				mmalcam.c
				mmaloutput.c
				motion.c
				netcam.c
				netcam_ftp.c
				netcam_jpeg.c
				netcam_wget.c
				metrics.c
				picture.c
				rotate.c
				stream.c
				track.c
				utils.c
				video2.c
				video.c
				video_common.c
				videosourceplugin.c
				vloopback_motion.c
				webhttpd.c
				raspicam/RaspiCamControl.c
				raspicam/RaspiCLI.c
				)

target_link_libraries(motion
						pthread
						m
						jpeg
						z
						rt
						bcm_host
						vcos
						vchiq_arm
						mmal_core
						mmal_util
						mmal_vc_client
						-Xlinker -rpath-link=${ROOTFSPATH}/usr/lib/arm-linux-gnueabihf -Xlinker -rpath-link=${ROOTFSPATH}/lib/arm-linux-gnueabihf
						)

