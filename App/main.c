/***************************************************************************** 
* 
* File Name : main.c
* 
* Description: main 
* 
* Copyright (c) 2014 Winner Micro Electronic Design Co., Ltd. 
* All rights reserved. 
* 
* Author : dave
* 
* Date : 2014-6-14
*****************************************************************************/ 
#include "wm_include.h"
#include "demo.h"

void UserMain(void)
{
	ayla_demo_init();	

#if DEMO_CONSOLE
	CreateDemoTask();
#endif
//用户自己的task
}

