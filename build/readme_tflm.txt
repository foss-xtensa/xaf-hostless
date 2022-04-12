======================================================================

This readme file describes instructions to build TensorFlow Lite For
Micro (TFLM) code for your HiFi core and use it for building and 
running TFLM applications under XAF. It also describes steps to add a 
new TFLM network as XAF component.

======================================================================
Xtensa tools version and Xtensa config requirements
======================================================================

RI2021.6 Tools (Xplorer 8.0.16)
Xtensa core must be using xclib (required for TFLM compilation)
xt-clang, xt-clang++ compiler (required for TFLM compilation)


======================================================================
Building TFLM code for xtensa target on Linux
======================================================================
Note: TFLM build requires the following GNU tools
    The versions recommnded are the ones on which it is tested
    GNU Make v3.82
    git v2.9.5
    wget v1.14
    curl v7.29

This section describes how to build the TFLM library to be used with 
XAF. Note that the TFLM compilation is only supported under Linux.

1.Copy /build/getTFLM.sh from XAF Package to the directory of choice 
  outside XAF Package under Linux environment. 
  This directory is referred to as <BASE_DIR> in the following steps.

2.Set up environment variables to have Xtensa Tools in $PATH and 
  $XTENSA_CORE(with xclib) defined to your HiFi core.

3.Execute getTFLM.sh <target>. This download and builds TFLM library in 
  <BASE_DIR/tensorflow>.  

  $ ./getTFLM.sh hifi3/hifi3z/hifi4/hifi5/fusion_f1

  Following libraries will be created in directory:
  For HiFi5 core:
  <BASE_DIR>/tensorflow/tensorflow/lite/micro/tools/make/gen/xtensa_hifi5_default/lib/
  For other cores: 
  <BASE_DIR>/tensorflow/tensorflow/lite/micro/tools/make/gen/xtensa_fusion_f1_default/lib/ 

  libtensorflow-microlite.a - TFLM Library
  libmicro_speech_frontend.a - Fronend lib for Microspeech Application

4.You can copy <BASE_DIR> directory from Linux to Windows for building 
  XAF Library and testbenches using Xplorer on Windows. In that case, 
  the destination directory on Windows is your new <BASE_DIR>.


======================================================================
Switching to person_detect application with XAF xws Package on Xplorer
======================================================================

Exclude: Right click on file, select Build, select Exclude.
Include: Right click on file, select Build, select Include.

By default, XAF xws package executes pcm gain application.
Following are the steps to build and test person detect application 
with XAF xws package.

1.Add component files
	Exclude the pcm-gain plug-in file
		testxa_af_hostless/test/plugins/cadence/pcm_gain/xa-pcm-gain.c
	Include the tflm person detection plug-in files
		testxa_af_hostless/test/plugins/cadence/tflm_common/tflm-inference-api.cpp
		testxa_af_hostless/test/plugins/cadence/tflm_common/xa-tflm-inference-api.c
		testxa_af_hostless/test/plugins/cadence/tflm_person_detect/person_detect_model_data.c
		testxa_af_hostless/test/plugins/cadence/tflm_person_detect/person-detect-wrapper-api.cpp

2.Changes to compile .cpp files
	Right click on file, select Build Properties, select Target as Common 
    in the new window that opens, select Language tab,and choose C++11 for 
    Language dialect
		testxa_af_hostless/test/plugins/cadence/tflm_common/tflm-inference-api.cpp
		testxa_af_hostless/test/plugins/cadence/tflm_person_detect/person-detect-wrapper-api.cpp
		
3.Add include paths
	Go to T:Release, select Modify, select Target as Common in the new 
    window that opens, and select 'Include Paths' tab
		Remove ${workspace_loc}/testxa_af_hostless/test/build/../../test/plugins/cadence/pcm_gain
		Add ${workspace_loc:testxa_af_hostless/test/plugins/cadence/tflm_common}
		Add ${workspace_loc:testxa_af_hostless/test/plugins/cadence/tflm_person_detect}
		Add <BASE_DIR>/tensorflow
		Add <BASE_DIR>/tensorflow/tensorflow/lite/micro/tools/make/downloads/flatbuffers/include

4.Add SYMBOLS
	Go to T:Release, select Modify, select Target as Common in the new 
    window that opens, and select 'Symbols' tab
		Remove XA_PCM_GAIN=1 symbol
		Add XA_TFLM_PERSON_DETECT symbol with value 1
		Add TF_LITE_STATIC_MEMORY symbol

5.Link the TFLM libraries
	Go to T:Release, select Modify, select Target as Common in the new 
    window that opens, and select 'Libraries' tab
		Add <BASE_DIR>/tensorflow/tensorflow/lite/micro/tools/make/gen/xtensa_$(target)_default/lib 
        under Library Search Paths(-L).
		Add tensorflow-microlite under Libraries(-l).

6.Additional linker option
  Go to T:Release, select Modify, select Target as Common in the new 
  window that opens, and select 'Addl linker' tab
  Add --gc-sections under Additional linker options.

7.Add test application files
	Exclude the pcm-gain application file
		testxa_af_hostless/test/src/xaf-pcm-gain-test.c
	Include the person-detect application file
		testxa_af_hostless/test/src/xaf-tflite-person-detect-test.c

8.Arguments to run:
	Go to Run, select Run Configurations, select af_hostless_cycle, and 
    replace the arguments 
	"-infile:test/test_inp/person_data.raw"


======================================================================
Switching to capturer + micro_speech application with XAF xws Package on Xplorer
======================================================================

Follow the steps similar to "Switching to person_detect application 
with XAF xws Package on Xplorer".
Exclude the current active application files (plug-in and test application) 
and remove include paths, SYMBOLS and libraries(if any) used for active 
application. Enable the required files for capturer + micro_speech application.

1.Component files list
		testxa_af_hostless/test/plugins/cadence/capturer/xa-capturer.c
		testxa_af_hostless/test/plugins/cadence/tflm_common/tflm-inference-api.cpp
		testxa_af_hostless/test/plugins/cadence/tflm_common/xa-tflm-inference-api.c
		testxa_af_hostless/test/plugins/cadence/tflm_microspeech/microspeech_model_data.c
		testxa_af_hostless/test/plugins/cadence/tflm_microspeech/microspeech-frontend-wrapper-api.cpp
		testxa_af_hostless/test/plugins/cadence/tflm_microspeech/microspeech-inference-wrapper-api.cpp
		testxa_af_hostless/test/plugins/cadence/tflm_microspeech/xa-microspeech-frontend.c

2..cpp files list
		testxa_af_hostless/test/plugins/cadence/tflm_common/tflm-inference-api.cpp
		testxa_af_hostless/test/plugins/cadence/tflm_microspeech/microspeech-frontend-wrapper-api.cpp
		testxa_af_hostless/test/plugins/cadence/tflm_microspeech/microspeech-inference-wrapper-api.cpp
		
3.include paths
		${workspace_loc:testxa_af_hostless/test/plugins/cadence/tflm_common}
		${workspace_loc:testxa_af_hostless/test/plugins/cadence/tflm_microspeech}
		<BASE_DIR>/tensorflow
		<BASE_DIR>/tensorflow/tensorflow/lite/micro/tools/make/downloads/flatbuffers/include

4.SYMBOLS list
		XA_CAPTURER symbol with value 1
		XA_TFLM_MICROSPEECH symbol with value 1
		TF_LITE_STATIC_MEMORY symbol

5.TFLM libraries list
		path: <BASE_DIR>/tensorflow/tensorflow/lite/micro/tools/make/gen/xtensa_$(target)_default/lib
		libraries: tensorflow-microlite and micro_speech_frontend

6.Additional linker option
    Add --gc-sections under Additional linker options.

7.test application file
		testxa_af_hostless/test/src/xaf-capturer-tflite-microspeech-test.c

8.arguments to run:
	"-outfile:test/test_out/out.pcm -samples:0"
	(copy the test/test_inp/capturer_in.pcm file to the testxa_af_hostless folder)


======================================================================
Switching to person_detect + micro_speech application with XAF xws Package on Xplorer
======================================================================

Follow the steps similar to "Switching to person_detect application with 
XAF xws Package on Xtensa Xplorer". Exclude the current active application 
files (plug-in and test application) and remove include paths, SYMBOLS 
and libraries(if any) used for active application.Enable the required 
files for capturer + micro_speech application.

1.Component files list
		testxa_af_hostless/test/plugins/cadence/capturer/xa-capturer.c
		testxa_af_hostless/test/plugins/cadence/tflm_common/tflm-inference-api.cpp
		testxa_af_hostless/test/plugins/cadence/tflm_common/xa-tflm-inference-api.c
		testxa_af_hostless/test/plugins/cadence/tflm_microspeech/microspeech_model_data.c
		testxa_af_hostless/test/plugins/cadence/tflm_microspeech/microspeech-frontend-wrapper-api.cpp
		testxa_af_hostless/test/plugins/cadence/tflm_microspeech/microspeech-inference-wrapper-api.cpp
		testxa_af_hostless/test/plugins/cadence/tflm_microspeech/xa-microspeech-frontend.c
		testxa_af_hostless/test/plugins/cadence/tflm_person_detect/person_detect_model_data.c
		testxa_af_hostless/test/plugins/cadence/tflm_person_detect/person-detect-wrapper-api.cpp
		
2..cpp files list
		testxa_af_hostless/test/plugins/cadence/tflm_common/tflm-inference-api.cpp
		testxa_af_hostless/test/plugins/cadence/tflm_microspeech/microspeech-frontend-wrapper-api.cpp
		testxa_af_hostless/test/plugins/cadence/tflm_microspeech/microspeech-inference-wrapper-api.cpp
		testxa_af_hostless/test/plugins/cadence/tflm_person_detect/person-detect-wrapper-api.cpp
		
3.include paths
		${workspace_loc:testxa_af_hostless/test/plugins/cadence/tflm_common}
		${workspace_loc:testxa_af_hostless/test/plugins/cadence/tflm_microspeech}
		${workspace_loc:testxa_af_hostless/test/plugins/cadence/tflm_person_detect}		
		<BASE_DIR>/tensorflow
		<BASE_DIR>/tensorflow/tensorflow/lite/micro/tools/make/downloads/flatbuffers/include

4.SYMBOLS list
		XA_CAPTURER symbol with value 1
		XA_TFLM_MICROSPEECH symbol with value 1
		XA_TFLM_PERSON_DETECT symbol with value 1
		TF_LITE_STATIC_MEMORY symbol

5.TFLM libraries list
		path: <BASE_DIR>/tensorflow/tensorflow/lite/micro/tools/make/gen/xtensa_$(target)_default/lib
		libraries: tensorflow-microlite and micro_speech_frontend

6.Additional linker option
    Add --gc-sections under Additional linker options.

7.test application file
		testxa_af_hostless/test/src/xaf-person-detect-microspeech-test.c

8.arguments to run:
	"-infile:test/test_inp/person_data.raw -outfile:test/test_out/out.pcm -samples:0"
	(copy the test/test_inp/capturer_in.pcm file to the testxa_af_hostless folder)


======================================================================
Steps to add new TFLM network as XAF component
======================================================================

1. Add file for the new TFLM network in plugins directory as below:

    test/plugins/cadence/tflm_<ntwrk>/
    ├── <ntwrk>_model_data.c
    ├── <ntwrk>_model_data.h
    └── <ntwrk>-wrapper-api.cpp

   Model data files should contain network model in flatbuffer format.
   Wrapper file mainly contains network spec initialization code and
   person-detect-wrapper-api.cpp/ microspeech-inference-wrapper-api.cpp
   should be used as template for new network wrapper.

2. Add entry for new plugin in xf_component_id list in 
    test/plugins/xa-factory.c as below:

   { "post-proc/<ntwrk>_inference", xa_audio_codec_factory, xa_<ntwrk>_inference },

3. Now, XAF component for the new TFLM network can be created, 
   connected to other components and executed from XAF application.


======================================================================
