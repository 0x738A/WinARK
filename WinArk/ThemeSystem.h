#pragma once
#include "ThemeColor.h"

extern COLORREF g_myColor[20];


struct t_scheme { // ��ɫ����
	std::string	name;          // ��������
	int  textcolor;            // �ı���ɫ
	int  hitextcolor;          // �ı�������ɫ
	int  lowcolor;             // �����ı���ɫ
	int  bkcolor;              // ������ɫ
	int  selbkcolor;           // ѡ��ʱ�ı�����ɫ
	int  linecolor;            // �ָ��ߵ���ɫ
	int  auxcolor;             // �����������ɫ
	int  condbkcolor;          // �����ϵ����ɫ
};

extern t_scheme g_myScheme[8];

enum {
	Black,
	Crimson,
	DarkGreen,
	DarkYellow,
	DarkBlue,
	DeepMagenta,
	DarkCyan,	// ����ɫ
	LightGray,
	DarkGray,
	Red,
	Green,
	Yellow,
	Blue,
	Magenta,
	Cyan,	// ����ɫ
	White,
	MoneyGreen,// ǳ��ɫ
	HackerGreen,
	Custom1,
	Custom2,
	Custom3
};

void InitColorSys();
void InitFontSys();
void InitPenSys();
void InitBrushSys();
void InitSchemSys();

enum class TableColorIndex {
	Text,
	HitText,
	Low,
	Bkg,
	SelBk,
	Line,
	Aux,
	CondBk,
	COUNT
};

bool SaveColors(PCWSTR path, PCWSTR prefix, const ThemeColor* colors, int count);
bool LoadColors(PCWSTR path, PCWSTR prefix, ThemeColor* colors, int count);
