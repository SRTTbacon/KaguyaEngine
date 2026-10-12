//*****************************************************************************
//
// ウィンドウの生成とゲームのメインループ
//
// Main.h
//
// T_Takeuchi
//
// 2026/5/24
//
//*****************************************************************************

#pragma once

#define BOOST_ALL_NO_LIB

enum WindowMode
{
	WindowNormal,		//通常のウィンドウ
	WindowMaximum,		//最大化されたウィンドウ
	WindowBorderLess,	//ボーダーレスウィンドウ (フルスクリーン)
	FullScreen			//フルスクリーン
};

int StartApp();