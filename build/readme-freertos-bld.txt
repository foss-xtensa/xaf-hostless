=======================================================================================================================
This file describes the steps required to replace XOS with FreeRTOS in XAF XWS package. 
=======================================================================================================================

A. Build the required FreeRTOS library for your HiFi4 core as below on Linux: 

1. Copy 'libxa_af_hostless/build/getFreeRTOS.sh' from XWS package to any directory outside XWS package <BASE_DIR>. 

2. Set up environment variables to have Xtensa Tools in PATH and XTENSA_CORE defined to <your_hifi4_core>.

3. Execute getFreeRTOS.sh in Linux environment, this should download and build FreeRTOS library in <BASE_DIR/FreeRTOS>.

4. You may copy <FreeRTOS> directory from Linux to Windows for steps B below. In that case, the destination directory 
   on Windows would be your new <BASE_DIR>.

=======================================================================================================================

B. Update the XWS package in Xplorer as below on Windows or Linux:

1. For 'libxa_af_hostless' project, update include paths for common target as below. 
   (Go to T:Debug, select Modify, select Target as Common in the new window that appears, select 'Include Paths' tab)

   Replace '${workspace_loc}/libxa_af_hostless/build/../include/sysdeps/xos/include' with 
   '${workspace_loc}/libxa_af_hostless/build/../include/sysdeps/freertos/include'

   Add following include paths
   <BASE_DIR>/FreeRTOS/lib/include
   <BASE_DIR>/FreeRTOS/lib/include/private
   <BASE_DIR>/FreeRTOS/lib/FreeRTOS/portable/XCC/Xtensa
   <BASE_DIR>/FreeRTOS/demos/cadence/sim/common/config_files
 
2. For 'libxa_af_hostless' project, update Symbols as below. 
   (Go to T:Debug, select Modify, select Target as Common in the new window that appears, select 'Symbols' tab)
 
   Replace 'HAVE_XOS' with 'HAVE_FREERTOS' in Defined Symbols list

3. For 'testxa_af_hostless' project, update include path for common target as below.
   (Go to T:Debug, select Modify, select Target as Common in the new window that appears, select 'Include Paths' tab)

    Replace '${workspace_loc}/libxa_af_hostless/include/sysdeps/xos/include' with 
    '${workspace_loc}/libxa_af_hostless/include/sysdeps/freertos/include'

    Add following include paths
    <BASE_DIR>/FreeRTOS/lib/include
    <BASE_DIR>/FreeRTOS/lib/include/private
    <BASE_DIR>/FreeRTOS/lib/FreeRTOS/portable/XCC/Xtensa
    <BASE_DIR>/FreeRTOS/demos/cadence/sim/common/config_files

4. For 'testxa_af_hostless' project, update Symbols as below. 
   (Go to T:Debug, select Modify, select Target as Common in the new window that appears, select 'Symbols' tab)

    Replace 'HAVE_XOS' with 'HAVE_FREERTOS' in Defined Symbols list

5. For 'testxa_af_hostless' project, update additional linker options as below. 
   (Go to T:Debug, select Modify, select Target as Common in the new window that appears, select 'Addl linker' tab)

    Replace '-lxos' in Additional linker options with 
    '-L<BASE_DIR>/FreeRTOS/demos/cadence/sim/build/<your_hifi4_core> -lfreertos'

6. Clean and Build  'testxa_af_hostless' project, it should now run with FreeRTOS.

7. To switch back to XOS, you should revert steps B - 1 to 5.

=======================================================================================================================
