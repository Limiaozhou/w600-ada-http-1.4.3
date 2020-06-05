## **一、**相关培训

前提有接受ayla的相关培训，清楚ayla设备连云的基本步骤以及相关概念，请联系ayla相关人员。

​	

## **二、**创建设备模板

更多相关请见如何创建设备模板.PDF
PDF介绍的是以智能插座为例的模板，DOC里自带本套SDK中ledevb的模板属性集，创建该模板在添加属性时，可直接导入该属性集。模板版本号:1.0-rtk

 

## **三、**申请设备PID

更多相关请见如何申请设备PID.PDF

 

## **四**、SDK编译

在此提供一种keil4编译环境，请自行注册或破解。

链接：https://pan.baidu.com/s/1a2yMJUEnzyivDcPLpYEMsQ 

提取码：k8qe 

 

## **五、工程配置**

SDK根目录\Tools\Keil\Project，用MDK472打开.uvproj工程，添加AIRKISS的宏定义。如图：

![img](file:///C:\Users\felix\AppData\Local\Temp\ksohtml10972\wps1.jpg) 



## **六、代码适配及编译**

​    在demo_ledevb.c找到#define BUILD_PID的宏定义，SDK已默认屏蔽，直接编译会报错。

​    1、将之设置为步骤二中申请的PID，打开宏定义。	

​    2、在build.h中将SUNSEA_PID的宏定义改为上述分配好的PID	

3、点击编译(编译结果在根目录bin文件夹，固件为.FLS文件和.GZ文件)

 

## **七、固件下载**

在此提供一种联盛德代理商星空智联下载方式，网址失效时请自行百度星空智联文档中心。
	链接： https://docs.w600.fun/?p=app/download.md  工具名：ThingsTurn_Serial_Tool

![img](file:///C:\Users\felix\AppData\Local\Temp\ksohtml10972\wps2.jpg) 

下载方式简介：

1、在固件地址栏选中SDK所在目录Bin文件夹中的FLS文件或者GZ文件。

（刚出厂的芯片或模组，首次需要选择FLS文件，往后烧录可只烧录GZ文件）

（擦除Flash的是否勾选取决于用户，擦除Flash后，下文中的ayla的配置参数需要重新写入，否则可反复使用ayla的配置参数）

2、点击下载。

3、手动复位设备或模组，自动进入下载。

 

 

## **八、**Ayla配置参数

工具下载地址：https://support.sunseaiot.com/SunseaIoT/Tools.git

（如无权限，请联系Ayla相关人员）

工具使用相关请见日海艾拉工具使用说明.PDF

 

## **九、**开通域名解析

在dashboard网站中(不是创建模板的develope)，找到创建的模板，点击申请开通dns解析。
目的：通过[oem_model]-[oem_id]-device.sunseaiot.com解析出Ayla云的ip




## **十、**配网说明

不同SDK对应不同产品功能，配网的操作可能略有差异，详情请见每个SDK根目录的README.md文档。

日海艾拉APP下载地址：https://www.showdoc.cc/242992023680781

中性APP下载地址：https://www.showdoc.cc/327107265946253

（请用手机端打开下载）

##  