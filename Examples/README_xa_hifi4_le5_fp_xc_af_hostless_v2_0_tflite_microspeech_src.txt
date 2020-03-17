=====================================================================================
XAF Hostless solution With Tensorflow Microspeech application for Yes/No recognition
=====================================================================================

+ Built with RI.2 tools and tested on AE_HiFI4_LE5_FP_XC core
+ XAF version 2.0 and tensorflow version (on top of this commit 62adf994a61435e74628a178dcd8845b99d73bfd, integrated hifi4 NN Lib v2.1 ) is used for the integration.
+ Current XAF pipeline is with three components
	 Capturer + tensorflow Microspeech Frond end processing + tensorflow Microspeech Inference
 	 Capturer runs at 20 ms framesize
	 Tensorflow Microspeech Frond end processing is running with 30 ms framesize and consumed will be 20 ms
	 tensorflow Microspeech Inference input size is 49x40 bytes (~one second worth feature data ) and each frame consumed will be 40 bytes.

+ Inference output at each frame is 8 byte data. 
	First 4 bytes value is either 0 or  1 . 1 means a new recognition is found. 
	Second 4 bytes value is 0,1,2 or 3.
		0 is for "Silence"
		1 is for "Unknown"
		2 is for "Yes"
		3 is for "No" 
		
	Sample code is given on xa_inference_do_execute_16bit function ( currently disabled ) 
	at location workspace_loc\testxa_af_hostless\test\plugins\cadence\tflite_microspeech\xa-inference.c
	
+ Import the project and build the target testxa_af_hostless  in Release mode (this would build the library projects libxa_af_hostless and libmicro_speech by default).
+ Default run is taking the input available at workspace_loc\testxa_af_hostless\capturer_in.pcm
 
 
