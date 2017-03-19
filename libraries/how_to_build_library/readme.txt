/**
  ******************** (C) COPYRIGHT 2017 MXCHIP MiCO SDK*******************
  * @文件  libraries/how_to_build_library/readme.txt 
  * @作者  李博(MiCO Documentation Working Group)
  * @版本  v3.2.x 
  * @日期  2017.02.15
  * @简介  如何从源码生成静态库
  ******************************************************************************
  
目录内容：
    - how_to_build_library/src/src.c.c                		组件的源代码
    - how_to_build_library/helloworld_lib.h 				组件的API接口头文件
    - how_to_build_library/how_to_build_library.mk  		用于编译应用的组件描述文件
    - how_to_build_library/how_to_build_library_src.mk      描述组件的源码构成
    - how_to_build_library/Makefile							将组件编译成静态库的编译脚本
    - how_to_build_library/readme.txt                       使用说明
    

组件介绍：
    - 这是一个组件模版，开发者可以直接将该组件以源码形式编译在应用程序中，也可以通过Makefile生成静态库。
    
如何运行这个组件：
    - Micoder工具：Micoder v1.2及以上      
		- cd <MiCO_SDK>
        - make helloworld_lib@<board>
    - IAR工具：IAR7.3.0及以上
    	- 应用中添加 demos/hellworld_lib 下的源代码，
    	- 应用中添加 libraries/how_to_build_library到搜索路径，
    
如果编译静态库：
    - Micoder工具：Micoder v1.2及以上      
        - cd <MiCO_SDK>
        - make -C libraries/how_to build_library HOST_ARCH=Cortex-M3, 生成Lib_Helloworld.Cortex-M3.GCC.release.a。
        - make -C libraries/how_to build_library HOST_ARCH=Cortex-M4, 生成Lib_Helloworld.Cortex-M4.GCC.release.a。
        - make -C libraries/how_to build_library HOST_ARCH=Cortex-M4F, 生成Lib_Helloworld.Cortex-M4F.GCC.release.a。
    - IAR工具：IAR7.3.0及以上
        - 打开IDE下的IAR工程文件
        - 选择预定义的Target文件，编译即可
      
使用自己的代码编译静态库： 
  - 生成GCC的静态库文件：
      - 将自己的源代码替代how_to build_library/src中的源代码。  
      - 将API接口头文件替换helloworld_lib.h。    
      - 打开how_to_build_library_src.mk文件，确定组件需要的各个源代码的位置。
      - 打开how_to_build_library.mk文件，将NAME修改为需要输出的组件的名称。
      - 打开Makefile文件，将NAME修改为需要输出的组件的名称，CFLAGS中添加必要的头文件搜索路径。
      - 重新编译
  
  - 生成IAR的静态库文件：
      - 将自己的源代码替代how_to build_library/src中的源代码，并且添加到IAR工程。  
      - 将API接口头文件替换helloworld_lib.h。 
      - 重新编译   
        
**/