#!/bin/sh

print_usage() {
    echo "Usage: $0 TARGET_ARCH"
    echo "TARGET_ARCH: hifi3 / hifi4 / hifi5 / fusion_f1"
    exit 1
}

if [ $# != 1 ] ; then
    print_usage
fi

if [ "$1" != "hifi3" ]  && [ "$1" != "hifi4" ] && [ "$1" != "hifi5" ] && [ "$1" != "fusion_f1" ] ; then
    print_usage
fi

if [ "$1" == "hifi5" ] ; then
target=$1
else
target="fusion_f1"
fi

command -v xt-clang >/dev/null || { echo "Xtensa Tools are not set up!"; exit 1; }

XTENSA_BASE=$(which xt-clang | sed 's/install.*/install/')
XTENSA_TOOLS_VERSION=$(which xt-clang | sed 's/.*tools\///' | sed 's/\/Xte.*//')

echo "using XTENSA_BASE $XTENSA_BASE"
echo "using XTENSA_TOOLS_VERSION $XTENSA_TOOLS_VERSION"
echo "using XTENSA_CORE $XTENSA_CORE"
echo "using TARGET_ARCH $target"

if [ -z "$XTENSA_BASE" ] | [ -z "$XTENSA_TOOLS_VERSION" ] | [ -z "$XTENSA_CORE" ]; then
    echo "Xtensa Tools or Xtensa Core not set properly!"
    exit 1
fi


MS_FE_OBJ_DIR=./tensorflow/lite/micro/tools/make/gen/xtensa_$target\_default/obj/tensorflow/lite/

echo "==> Clone TFLM source tree"
git clone git://github.com/tensorflow/tensorflow.git
cd tensorflow
git checkout a1acddc # https://github.com/tensorflow/tensorflow/commit/a1acddcf3f990332b5f0bd19faba1d6e256787f0

echo "==> Make TFLM third-party-downlaods"
echo "make third_party_downloads -f tensorflow/lite/micro/tools/make/Makefile TARGET=xtensa OPTIMIZED_KERNEL_DIR=xtensa TARGET_ARCH=$target XTENSA_BASE=$XTENSA_BASE XTENSA_TOOLS_VERSION=$XTENSA_TOOLS_VERSION XTENSA_CORE=$XTENSA_CORE"
make third_party_downloads -f tensorflow/lite/micro/tools/make/Makefile TARGET=xtensa OPTIMIZED_KERNEL_DIR=xtensa TARGET_ARCH=$target XTENSA_BASE=$XTENSA_BASE XTENSA_TOOLS_VERSION=$XTENSA_TOOLS_VERSION XTENSA_CORE=$XTENSA_CORE

echo "==> Build TFLM library and micro-speech"
echo "make -f tensorflow/lite/micro/tools/make/Makefile TARGET=xtensa OPTIMIZED_KERNEL_DIR=xtensa TARGET_ARCH=$target XTENSA_BASE=$XTENSA_BASE XTENSA_TOOLS_VERSION=$XTENSA_TOOLS_VERSION XTENSA_CORE=$XTENSA_CORE micro_speech -j"
make -f tensorflow/lite/micro/tools/make/Makefile TARGET=xtensa OPTIMIZED_KERNEL_DIR=xtensa TARGET_ARCH=$target XTENSA_BASE=$XTENSA_BASE XTENSA_TOOLS_VERSION=$XTENSA_TOOLS_VERSION XTENSA_CORE=$XTENSA_CORE micro_speech -j

echo "==> Create micro-speech frontend library"
xt-ar -r $MS_FE_OBJ_DIR/../../../lib/libmicro_speech_frontend.a $MS_FE_OBJ_DIR/micro/examples/micro_speech/audio_provider.o $MS_FE_OBJ_DIR/micro/examples/micro_speech/feature_provider.o $MS_FE_OBJ_DIR/micro/examples/micro_speech/micro_features/no_micro_features_data.o $MS_FE_OBJ_DIR/micro/examples/micro_speech/micro_features/yes_micro_features_data.o $MS_FE_OBJ_DIR/micro/examples/micro_speech/recognize_commands.o $MS_FE_OBJ_DIR/micro/examples/micro_speech/command_responder.o $MS_FE_OBJ_DIR/micro/examples/micro_speech/micro_features/micro_features_generator.o $MS_FE_OBJ_DIR/micro/examples/micro_speech/micro_features/micro_model_settings.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/fft.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/fft_util.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/filterbank.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/filterbank_util.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/frontend.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/frontend_util.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/log_lut.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/log_scale.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/log_scale_util.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/noise_reduction.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/noise_reduction_util.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/pcan_gain_control.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/pcan_gain_control_util.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/window.o $MS_FE_OBJ_DIR/experimental/microfrontend/lib/window_util.o $MS_FE_OBJ_DIR/micro/tools/make/downloads/kissfft/kiss_fft.o $MS_FE_OBJ_DIR/micro/tools/make/downloads/kissfft/tools/kiss_fftr.o
echo "==> Done!"
