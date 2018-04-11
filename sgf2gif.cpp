// MIT License
//
// Copyright (c) 2016 C.T.Chen
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// 注意: 以上版权声明只适用于本文件(sgf2gif.cc), 不适用于 3rdpart 目录下的文件.

#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <Shlwapi.h>
#include <CommCtrl.h>
#include <vector>
#include <string>
#include <assert.h>
#include <gdiplus.h>
#include <stdio.h>
#include <stdlib.h>


#include "3rdpart/CxImage/ximage.h"
#include "3rdpart/CxImage/ximagif.h"
#include "3rdpart/simplexnoise1234.h"
#include "3rdpart/Quantize.h"

#include "res/resource.h"


// #define S2F_RENJU_ONLY
// #define S2F_ENG // 使用英文标题

#ifdef S2F_ENG
#	define STR_WHITE L"W"
#	define STR_BLACK L"B"
#	define STR_VS L" "
#	define STR_UNKOWN L"Unkown"
#	define STR_WIN L"+"
#	define STR_WIN_BY_RESIGN L"+R"
#
#else
# define STR_WHITE L"白"
#	define STR_BLACK L"黑"
#	define STR_VS L" "
#	define STR_UNKOWN L"未知"
#	define STR_WIN L"胜"
#	define STR_WIN_BY_RESIGN L"中盘胜"
#endif

using namespace Gdiplus;
using namespace std;

const int MARGIN = 4; // 边距, 防止内容紧贴着图片边界
const int TITLE_HEIGHT = 20;
const int LEFT_SPACE_WIDTH = 0;

const int MAX_BOARD_SIZE = 27;
const int MAX_MOVE = MAX_BOARD_SIZE * MAX_BOARD_SIZE * 4; // ugly, but works

const int BOARD_LEFT = LEFT_SPACE_WIDTH + MARGIN;
const int BOARD_TOP = TITLE_HEIGHT + MARGIN;

// 选项对话框里的内容, 最开始的时候只需记简单的信息, 现在要记得东西多了, 已经有点不适用.
struct Options
{
	int delay; // 延迟, 即播放速度
	int numbers; // 最后的多少手棋显示手数
	bool splitPeriodically; // 是否分割
	int splitCount; // 分割点的个数
	int splitPoints[20]; // 分割点
	int cw; // cell width, 棋子尺寸

	// 是否真的要分割图片
	bool RealSplit()
	{
		return splitPeriodically || splitCount > 0;
	}

	// 延迟转为字符串
	string GetDelayString()
	{
		char buf[32];
		sprintf(buf, "%d", delay);
		return buf;
	}

	void SetDelayString(const string & s)
	{
		delay = 50;
		delay = atoi(s.c_str());
		if(delay <0)
		{
			delay = 0;
		}
	}

	string GetNumbersString()
	{
		char buf[32];
		sprintf(buf, "%d", numbers);
		return buf;
	}

	void SetNumbersString(const string & s)
	{
		numbers = 1;
		numbers = atoi(s.c_str());
	}

	string GetSplitString()
	{
		if(splitPeriodically)
		{
			char buf[32];
			sprintf(buf, "*%d", splitPoints[0]);
			return buf;
		}
		else
		{
			string ret;
			for(int i=0; i<splitCount; i++)
			{
				char buf[32];
				sprintf(buf, "%d ", splitPoints[i]);
				ret+= buf;
			}
			if(ret.length() > 0)
			{
				// ret.pop_back(); pop_back unavailable before c++11 standard
				ret.resize(ret.length() - 1);
			}
			return ret;
		}
	}

	void SetSplitString(const string & s)
	{
		if(s.empty())
		{
			splitPeriodically = false;
			splitCount = 0;
			return;
		}
		if(s[0] == '*')
		{
			splitPeriodically = true;
			splitCount = 1;
			splitPoints[0] = 50;
			if(s.length() > 1)
			{
				splitPoints[0] = atoi(&s[1]);
				if(splitPoints[0] < 5)
				{
					splitPoints[0] = 5;
				}
			}
		}
		else
		{
			splitPeriodically = false;
			splitCount = 0;
			string tmp;
			int n = 0;
			for(int i=0; i<s.length() && splitCount<20;)
			{
				tmp.clear();
				while(i<s.length() && (s[i] <= 32 || s[i] ==',' || s[i] == '|'))
				{
					i++;
				}
				while(i<s.length() && s[i] > 32 && s[i] != ',' && s[i] !='|' )
				{
					tmp.push_back(s[i]);
					i++;
				}
				int n1 = atoi(tmp.c_str());
				if(n1 > n + 5)
				{
					n = n1;
					splitPoints[splitCount] = n;
					splitCount++;
				}
			}
		}
	}

	string GetCWString()
	{
		char buf[32];
		sprintf(buf, "%d", cw);
		return buf;
	}

	void SetCWString(const string & s)
	{
		cw = 23;
		cw = atoi(s.c_str());

		if(cw < 15)
		{
			cw = 15;
		}
		else if(cw > 50)
		{
			cw = 50;
		}
	}

} g_options;


int CW() // cell width (pixels)
{
	return g_options.cw;
}

int BW(int BS) // board width (pixels)
{
	return BOARD_LEFT + CW() * BS + MARGIN + 16;
}
int BH(int BS) // board height (pixels)
{
	return BOARD_TOP + CW() * BS + MARGIN + 16;
}

enum StoneColor
{
	None = 0,
	Black = 1,
	White = 2,
};

// 棋盘上的交叉点
struct Cross
{
	char x;
	char y;
};

struct Move : public Cross
{
	bool addition; // 是否座子
	StoneColor color;
	Move()
	{
		color = None;
		addition = false;
		x = 0;
		y = 0;
	}
};


struct Game
{
	// 棋谱类型, 参见SFG官网的定义, GM[x]. 我们只支持五子棋和围棋
	enum GameType
	{
		gameUnkown = 0,
		gameGo = 1,
		gameRenju = 4

	}gameType;

	int ca; // encoding
	int boardSize;
	StoneColor winner;
	bool winByResign; // 中盘胜, +R

	wstring gameTitle;
	wstring komi; // 贴目, 用处不大
	wstring result; // 对弈结果, 文本

	wstring whiteName;
	wstring whiteRank;

	wstring blackName;
	wstring blackRank;

	int moveCount; // 当前的步数
	Move moves[MAX_MOVE];

	Game()
	{
		ca = 0;
		winner = None;
		winByResign = false;
		moveCount = 0;
		boardSize = 19;
#ifdef S2F_RENJU_ONLY
		gameType = gameRenju;
#else
		gameType = gameGo;
#endif
	}

	wstring GetTitle()
	{
		wstring ret;
		if (!gameTitle.empty()) {
			return gameTitle;
		}
		if(!whiteName.empty() || !blackName.empty())
		{
			ret +=  whiteName.empty() ? STR_WHITE L": " STR_UNKOWN : STR_WHITE L": " + whiteName;
			ret += whiteRank.empty() ? L"" : L" " + whiteRank + L"";
			ret += STR_VS;
			ret += blackName.empty() ? STR_BLACK L": " STR_UNKOWN : STR_BLACK L": " + blackName;;
			ret += blackRank.empty() ? L"" : L" " + blackRank + L"";
			//ret += L" ";
		}


		if(winByResign)
		{
			if(winner == White)
			{
				ret+= STR_WHITE STR_WIN_BY_RESIGN;
			}
			else if(winner == Black)
			{
				ret+= STR_BLACK STR_WIN_BY_RESIGN;
			}
		}
		else
		{
			if(winner == White)
			{
				ret+= STR_WHITE STR_WIN;
			}
			else if(winner == Black)
			{
				ret+= STR_BLACK STR_WIN;
			}

			if(!result.empty())
			{
				ret+= result;
			}
		}

		//if(!komi.empty())
		//{
		//	ret+= " 贴目: " + komi;
		//}
		return ret;
	}

	int GetRealMoveCount()
	{
		int n;
		for(n = 0; n < moveCount && moves[n].addition; n++)
		{}
		return moveCount - n;
	}

	wstring ReadStringValue(const string & src, int & i);
};

string RemoveExtension(const string & src)
{
	if(src.length() > 4 && src[src.length() - 4] == '.')
	{
		return src.substr(0, src.length() - 4);
	}
	else
	{
		return src;
	}
}

string ReadFileIntoString(const string & srcPath)
{
	string ret;
	FILE * file = fopen(srcPath.c_str(), "r");
	if(!file)
	{
		return ret;
	}
	while(!feof(file))
	{
		char c = fgetc(file);
		ret.push_back(c);
	}
	fclose(file);
	return ret;
}

// 解释落子坐标, SGF里用的是字母
bool ReadMove(const string & src, int & i, Move & m)
{
	while(i < src.size() && src[i] <= 32)
	{
		i++;
	}

	if(i >= src.size() || src[i] != '[')
	{
		return false;
	}

	i++;
	if (src[i] == ']') {
		m.x = -1;
		m.y = -1;
		i++;
		return true;
	}
	if(i < src.size() && src[i] >= 'a' && src[i] <= 'z')
	{
		m.x = src[i] - 'a';
		i++;
		if(i < src.size() && src[i] >= 'a' && src[i] <= 'z')
		{
			m.y = src[i] - 'a';
			while(i < src.size() && src[i] != ']')
			{
				i++;
			}
			i++;
			return true;
		}
	}

	return false;
}

// 跳过未知的值
bool SkipValue(const string & src, int & i)
{
	while(i < src.size() && src[i] <= 32)
	{
		i++;
	}

	if(i >= src.size() || src[i] != '[')
	{
		return false;
	}
	i++;

	while(i < src.size() && src[i] != ']')
	{
		i++;
	}
	if (src[i] == ']') {
		i++;
		return true;
	}
	return false;
}

wstring mbs_to_ws(UINT cp, const char * p)
{
	wstring dst;
	int i = MultiByteToWideChar(cp, 0, p, -1, NULL, 0);
	dst.resize(i + 1);
	MultiByteToWideChar(cp, 0, p, -1, (wchar_t*)dst.data(), i);
	return dst;
}

wstring mbs_to_ws(UINT cp, const string& str)
{
	return mbs_to_ws(cp, str.c_str());
}



// 读字符串, 参见SGF官方定义
// 这个函数大概有问题, 至少是没有处理UTF,BIG5等各种编码
wstring Game::ReadStringValue(const string & src, int & i)
{
	string ret;
	while(i < src.size() && src[i] <= 32)
	{
		i++;
	}

	if(i < src.size() && src[i] == '[')
	{
		i++;

		while(i < src.size() && src[i] != ']')
		{
			if(src[i] == '\\')
			{
				i++;
				if(i < src.size())
				{
					ret.push_back(src[i]);
				}
			}
			else
			{
				ret.push_back(src[i]);
				if(src[i] < 0 && i + 1 < src.size())
				{
					i++;
					ret.push_back(src[i]);
				}
			}
			i++;
		}
		i++;
	}
	return mbs_to_ws(this->ca, ret);
}

// 解释SGF格式.
// (为啥不用第三方库? 因为我不会用.)
bool LoadSimpleSGF(Game & game, const string & srcPath)
{
	bool bReadSize = false;
	// 暂时把座子也算在Move里
	string src = ReadFileIntoString(srcPath);
	if(src.empty())
	{
		return false;
	}
	int i = 0;
	// 跳过 (;
	while(i < src.size() && ( src[i] <= 32 || src[i] == '(' || src[i] == ';'))
	{
		i++;
	}

	// 依次读标志, 直到遇到右括号
	while(i < src.size() && src[i] != ')')
	{
#ifdef _DEBUG
		const char * tmp = &src[i];
#endif
		while(i < src.size() && (src[i] <= 32 || src[i] == ';' || src[i] == '('))
		{
			i++;
		}

		string propName;

		while(i < src.size() && src[i] >= 'A' && src[i] <= 'Z')
		{
			propName.push_back(src[i++]);
		}

		if(propName == "") {
			return false;
		}

#ifdef _DEBUG
		printf("debug: read prop: %s @%d\n", propName.c_str(), tmp - &src[0]);
#endif
		if(propName == "GM")
		{
			// 棋种, 棋谱类型
			wstring s = game.ReadStringValue(src, i);
			int t = s.empty() ? 1 : _wtoi(s.c_str());
			if(t == 4)
			{
				game.gameType = Game::gameRenju;
				if(!bReadSize)
				{
					game.boardSize = 15;
				}
				printf("Game type is GM[%d] Renju.\n", t);
			}
			else
			{
#ifdef S2F_RENJU_ONLY

				printf("Unsupported game type : %d, treat as Renju.\n", t);
				game.gameType = Game::gameRenju;
				if(!bReadSize)
				{
					game.boardSize = 15;
				}

#else
				if(t != 1)
				{
					printf("Unsupported game type : %d, treat as Weiqi.\n", t);
				}
				else
				{
					printf("Game type is GM[%d] Weiqi.\n", t);
				}
				game.gameType = Game::gameGo;
				if(!bReadSize)
				{
					game.boardSize = 19;
				}
#endif
			}

		}
		else if(propName == "AB" || propName == "AW" || propName == "B" || propName == "W")
		{
			// 座子/落子坐标
			Move m;
			m.color = propName == "AB"  || propName == "B" ? Black : White;
			m.addition = propName == "AB" || propName == "AW" ? true : false;
			while(ReadMove(src, i, m))
			{
				game.moves[game.moveCount++] = m;
			}
		}
		else if(propName == "SZ")
		{
			// 棋盘大小. 围棋一般是19/13/9, 五子棋15. 但也有例外
			bReadSize = true;
			wstring s = game.ReadStringValue(src, i);
			if(!s.empty())
			{
				game.boardSize = _wtoi(s.c_str());
			}
			if(game.boardSize < 3 || game.boardSize > MAX_BOARD_SIZE)
			{
				game.boardSize = game.gameType == Game::gameRenju ? 15 : 19;
			}

		}
		else if(propName == "PB")
		{
			// 黑方棋手
			game.blackName = game.ReadStringValue(src, i);
		}
		else if(propName == "PW")
		{
			// 白方棋手
			game.whiteName = game.ReadStringValue(src, i);
		}
		else if(propName == "BR")
		{
			// 黑方段位
			game.blackRank = game.ReadStringValue(src, i);
		}
		else if(propName == "WR")
		{
			// 白方段位
			game.whiteRank = game.ReadStringValue(src, i);
		}
		else if(propName == "KM")
		{
			// 贴目
			game.komi = game.ReadStringValue(src, i);
		}
		else if(propName == "RE")
		{
			// 对局结果
			// [B+R]     黑中盘胜
			// [W+R]     白中盘胜
			// [B+0.5]   黑胜0.5目
			// [W+100]   白胜100目
			// [其他]    其他结果, 例如"打挂", "无胜负"等
			wstring re =  game.ReadStringValue(src, i);
			if(re.size() >= 2 && re[1] == '+')
			{
				if(re[0] == 'B' || re[0] == 'b')
				{
					game.winner = Black;
				}
				else if(re[0] == 'W' || re[0] == 'w')
				{
					game.winner = White;
				}

				if(re.size() >= 3 )
				{
					if(re[2] == 'R' || re[2] == 'r')
					{
						game.winByResign = true;
					}
					game.result = re.substr(2, -1);
				}
				else
				{
					game.result = re;
				}
			}
			else
			{
				game.result = re;
			}
		}
		else if(propName == "CA")
		{
			// 按官网的定义, CA属性内容是RFC1345里定义的编码页, 但RFC1345全文没有""UTF"相关的内容.
			// 估计实际的软件并不完全遵守这个规则, 我们手头又没有足够的资料, 有点抓瞎.
			// 目前我们采用模糊匹配的方法来猜测, 如果检测不出就默认为操作系统的编码页.

			wstring re = game.ReadStringValue(src, i);
			if (StrStrIW(re.c_str(), L"utf") != NULL && StrStrIW(re.c_str(), L"8") != NULL) {
				//  UTF-8
				game.ca = CP_UTF8;
			} else if (StrStrIW(re.c_str(), L"gb") != NULL || StrStrIW(re.c_str(), L"936") != NULL) {
				//  GBK
				game.ca = 936;
			} else if ((StrStrIW(re.c_str(), L"big") != NULL && StrStrIW(re.c_str(), L"5") != NULL) || StrStrIW(re.c_str(), L"950") != NULL) {
				//  BIG5
				game.ca = 950;
			} else if ((StrStrIW(re.c_str(), L"shift") != NULL && StrStrIW(re.c_str(), L"jis") != NULL) || StrStrIW(re.c_str(), L"932") != NULL) {
				//  SHIFT_JIS
				game.ca = 932;
			} else if ((StrStrIW(re.c_str(), L"euc") != NULL && StrStrIW(re.c_str(), L"jp") != NULL) || StrStrIW(re.c_str(), L"51932") != NULL) {
				//  EUC_JP
				game.ca = 51932;
			} else if ((StrStrIW(re.c_str(), L"euc") != NULL && StrStrIW(re.c_str(), L"kr") != NULL) || StrStrIW(re.c_str(), L"51949") != NULL) {
				//  EUC_KR
				game.ca = 51949;
			} else {
				printf("unkown CA prop, treat as locale MBS\n");
				game.ca = CP_ACP;
			}
		}
		else if(propName == "GN")
		{
			// 棋谱名称?
			game.gameTitle = game.ReadStringValue(src, i);
		}
		else if(propName == "C")
		{
			// 注解, 注释, 备注
			game.ReadStringValue(src, i);
			printf("ignore comment at move %d\n", game.moveCount);
		}
		else
		{
			// 其他属性直接忽略, 例如三角符号, A,B,C,D等分支标识等, 这些东西太费事了.
			printf("Ignore property : %s\n", propName.c_str());
			while(SkipValue(src, i))
			{
				// do nothing
			}
		}
	}

	return true;
}


// 种子填充算法, 数气, 此函数不是特别慢, 没必要优化
bool HasLiberty(int BS, StoneColor board[MAX_BOARD_SIZE][MAX_BOARD_SIZE], bool flags[MAX_BOARD_SIZE][MAX_BOARD_SIZE], int x, int y)
{
	flags[x][y] = true;
	if(x <(BS-1))
	{
		if( board[x][y] == board[x+1][y] && !flags[x+1][y])
		{
			if(HasLiberty(BS, board, flags, x + 1, y))
			{
				return true;
			}
		}
		else if(board[x+1][y] == None)
		{
			return true;
		}
	}
	if(x >0)
	{
		if( board[x][y] == board[x-1][y] && !flags[x-1][y])
		{
			if( HasLiberty(BS, board, flags, x - 1, y))
			{
				return true;
			}
		}
		else if(board[x-1][y] == None)
		{
			return true;
		}
	}

	if(y <(BS-1))
	{
		if( board[x][y] == board[x][y+1] && !flags[x][y+1])
		{
			if( HasLiberty(BS, board, flags, x, y+1))
			{
				return true;
			}
		}
		else if(board[x][y+1] == None)
		{
			return true;
		}
	}
	if(y > 0)
	{
		if( board[x][y] == board[x][y-1] && !flags[x][y-1])
		{
			if( HasLiberty(BS, board, flags, x, y-1))
			{
				return true;
			}
		}
		else if(board[x][y-1] == None)
		{
			return true;
		}
	}
	return false;
}


// 检测是否需要提子
void CheckTakeStone(int BS, StoneColor board[MAX_BOARD_SIZE][MAX_BOARD_SIZE], int x, int y)
{
	bool flags[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
	memset(flags, 0, sizeof(flags));
	StoneColor c = board[x][y];
	if(!HasLiberty(BS, board, flags, x, y))
	{
		for(int i=0; i<BS; i++)
		{
			for(int j=0; j<BS; j++)
			{
				if(flags[i][j])
				{
					assert(board[i][j] == c); // 原本的颜色一定和检测点的相同, 因为是同一块棋
					board[i][j] = None; // 提子
				}
			}
		}
	}
}

// 五子棋落子函数, 不需要提子
bool PutStoneRenju(int BS, StoneColor board[MAX_BOARD_SIZE][MAX_BOARD_SIZE], const Move & m)
{
	if(m.color == None)
	{
		return false;
	}

	if(m.x < 0 || m.y < 0|| m.x >= BS || m.y >= BS)
	{
		printf("Coord is out of board, treat as pass.\n");
		return false;
	}

	board[m.x][m.y] = m.color;
	return true;
}

// 围棋落子函数, 需要检测提子
bool PutStoneWeiqi(int BS, StoneColor board[MAX_BOARD_SIZE][MAX_BOARD_SIZE], const Move & m)
{
	if(m.color == None)
	{
		return false;
	}

	if(m.x < 0 || m.y < 0|| m.x >= BS || m.y >= BS)
	{
		printf("Coord is out of board, treat as pass.\n");
		return false;
	}

	board[m.x][m.y] = m.color;

	// 四个方向的棋都需要检测是否气尽
	if(m.x < (BS-1) && m.color != board[m.x+1][m.y])
	{
		CheckTakeStone(BS, board, m.x + 1, m.y);
	}

	if(m.x > 0 && m.color != board[m.x-1][m.y])
	{
		CheckTakeStone(BS, board, m.x - 1, m.y);
	}

	if(m.y < (BS-1) && m.color != board[m.x][m.y+1])
	{
		CheckTakeStone(BS,board, m.x, m.y+1);
	}

	if(m.y > 0 && m.color != board[m.x][m.y-1])
	{
		CheckTakeStone(BS,board, m.x, m.y-1);
	}

	// 按目前的规则似乎不需要检测自提

	return true;
}

bool PutStone(Game::GameType gt, int BS, StoneColor board[MAX_BOARD_SIZE][MAX_BOARD_SIZE], const Move & m)
{
	if(gt == Game::gameGo)
	{
		return PutStoneWeiqi(BS, board, m);
	}
	else
	{
		return PutStoneRenju(BS, board, m);
	}
}

static HFONT CreateNativeFont(const string & _fontName, int _fontSize, bool _bold)
{
	if(_fontSize < 1)
	{
		_fontSize = 1;
	}
	LOGFONT lf;
	memset(&lf, 0, sizeof(lf));

	lf.lfHeight = -_fontSize;
	lf.lfWidth = 0;
	lf.lfEscapement = 0;
	lf.lfOrientation = 0;
	if(_bold)
	{
		lf.lfWeight = FW_BOLD;
	}
	else
	{
		lf.lfWeight = FW_NORMAL;
	}

	//lf.lfItalic    = _font.italic ? TRUE : FALSE;
	//lf.lfUnderline = _font.underline ? TRUE : FALSE;
	//lf.lfStrikeOut = _font.strikethrough ? TRUE : FALSE;
	lf.lfCharSet   = DEFAULT_CHARSET;
	lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = ANTIALIASED_QUALITY;
	lf.lfPitchAndFamily = DEFAULT_PITCH;
	if(_fontName.length() > 32)
	{
#	ifdef UNICODE
		wcscpy(lf.lfFaceName, L"SYSTEM");
#	else
		strcpy(lf.lfFaceName, "SYSTEM");
#	endif
	}
	else
	{
#	ifdef UNICODE
		wcscpy(lf.lfFaceName, _fontName.c_str());
#	else
		strcpy(lf.lfFaceName, _fontName.c_str());
#	endif
	}
	return ::CreateFontIndirect(&lf);
}

bool IsStar(int bs, int x, int y)
{
	switch(bs)
	{
	case 9:
		return (x == 2 || x == 6) && (y == 2 || y == 6);
	case 13:
		return ((x == 3 || x == 9) && (y == 3 || y == 9)) || (x == 6 && y == 6);
	case 19:
		return (x == 3 || x == 9 || x == 15) && (y == 3 || y == 9 || y == 15);
	default:
		return false;
	}
}

// 画格子, 有棋子画棋子, 没有棋子画空交叉点.
// 注意图片是不透明的, 原先有棋子的地方被空交叉点覆盖就是提子的效果
void DrawCell(HDC hDC,  int BS, StoneColor color, int i, int j, int x0, int y0, int num = 0)
{
	Graphics * g = Graphics::FromHDC(hDC);
	Status status = g->SetSmoothingMode( Gdiplus::SmoothingModeAntiAlias );
	g->SetPageUnit( Gdiplus::UnitPixel );
	if(color == White || color == Black)
	{
		// 画棋子
		if(color == White)
		{
			::SelectObject(hDC, ::GetStockObject(WHITE_BRUSH));
			::SetTextColor(hDC, RGB(0, 0, 0));
			SolidBrush brush(Color::White);
			g->FillEllipse(&brush, x0+1 , y0+1  , CW()-2, CW()-2);
		}
		else
		{
			::SelectObject(hDC, ::GetStockObject(BLACK_BRUSH));
			::SetTextColor(hDC, RGB(255, 255, 255));
			SolidBrush brush(Color::Black);
			g->FillEllipse(&brush, x0+1 , y0+1 , CW()-2, CW()-2);
		}
		Pen pen(Color::Black);
		g->DrawEllipse(&pen, x0+1 , y0+1  , CW()-2, CW()-2);

		if(num > 0)
		{
			// 画手数
			char buf[100];
			sprintf(buf, "%d", num);
			RECT rect = {x0+1, y0, x0 + CW(), y0 + CW()};
			::DrawText(hDC, buf, strlen(buf), &rect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
		}
	}
	else
	{
		// 画空交叉点
		int cx = x0 + CW() / 2;
		int cy = y0 + CW() / 2;
		int l, t, r, b;
		l = i > 0 ? x0 : cx;
		t = j > 0 ? y0 : cy;
		r = i < (BS-1) ?  x0 + CW() : cx;
		b = j < (BS-1) ?  y0 + CW() : cy;
		::MoveToEx(hDC, l, cy, NULL);
		::LineTo(hDC, r, cy);
		::MoveToEx(hDC, cx, t, NULL);
		::LineTo(hDC, cx, b);

		if(IsStar(BS, i, j))
		{
			// 画星位标记
			::SelectObject(hDC, ::GetStockObject(BLACK_BRUSH));
			::Rectangle(hDC, cx-1, cy-1 , cx+2, cy+2);
		}
	}
	g->ReleaseHDC( hDC );
	delete g;

}

float frand()
{
	return float(rand()) / float(RAND_MAX);
}

// 棋盘底纹是过程纹理, 在GIF里横向纹理压缩率高
void DrawBoardBG(HDC hDC, RECT rect)
{
	float x0 = frand();
	float y0 = frand();
	float sx = frand() * 0.00001f + 0.00007f;
	float sy = frand() * 0.1f + 0.07f;
	for(int i=rect.left; i<rect.right; i++)
	{
		for(int j=rect.top; j<rect.bottom; j++)
		{
			float x = (i) * sx + 1 + x0;
			float y = (j) * sy  + 1 + y0;
			float v = 0;
			for(int k=1; k<5; k++)
			{
				float m = float(1 << k);
				float v0 = SimplexNoise1234::noise(x*m, y*m) / m;
				//v0 = fabs(v0);
				v+= v0;
			}
			//float v = SimplexNoise1234::noise(x * 0.001f, y * 1.0f);
			//v/=1.5;
			v = (v+ 1.0f) * 0.5f;

			int r = v * 30 + 195; //210
			int g = v * 15 + 177; // 184
			int b = v * 8 + 134; // 140
			::SetPixel(hDC, i, j, RGB(r, g, b));
		}
	}
}


// 把从firstNum到lastNum的着法转换成一个GIF
bool Convert(Game & game, const string & dstPath, int firstNum, int lastNum)
{
	int BS = game.boardSize;
	StoneColor preBoard[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
	memset(preBoard, 0, sizeof(preBoard));
	StoneColor board[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
	memset(board, 0, sizeof(board));
	HDC hDC = ::CreateCompatibleDC(NULL);
	//Graphics * g = Graphics::FromHDC(hDC);
	COLORREF bgColor = RGB(255, 255, 255);
	HBRUSH hBgBrush = ::CreateSolidBrush(bgColor);
	HBRUSH hOldBrush = (HBRUSH) ::SelectObject(hDC, hBgBrush);
	HFONT hMonoFont = CreateNativeFont("Lucida Console", 10, false);
	HFONT hNumFont = CreateNativeFont("Tahoma", 10, false);
	HFONT hTitleFont = CreateNativeFont("Tahoma", 14, false);
	HFONT hOldFont = (HFONT) ::SelectObject(hDC, hNumFont);
	::SetTextColor(hDC, RGB(0, 0, 0));
	::SetBkMode(hDC, TRANSPARENT);

	// 单个格子大小的位图, 里面的内容是随画随用
	HBITMAP hBmpCell = ::CreateBitmap(CW(), CW(), 1, 32, NULL);

	// 棋盘位图
	HBITMAP hBmpBoard = ::CreateBitmap(BW(BS), BH(BS), 1, 32, NULL);

	// 先把棋盘位图选入hDC, 在位图上话东西
	HBITMAP hOldBmp = (HBITMAP)::SelectObject(hDC, hBmpBoard);

	// 画底纹
	RECT rect= { 0, 0, BW(BS), BH(BS) };
	DrawBoardBG(hDC, rect);

	// 画标题, 标题一直不变, 画在底图上就行
	wstring title = game.GetTitle();
	::SelectObject(hDC, hTitleFont);
	rect.bottom = BOARD_TOP;
	::DrawTextW(hDC, title.c_str(), title.length(), &rect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

	for(int i=0; i< BS; i++)
	{
		// 画坐标
		char buf[32];

		rect.left = BOARD_LEFT + CW() * BS + 4;
		rect.right = BW(BS) - 2;
		rect.top = BOARD_TOP + CW() * i;
		rect.bottom = rect.top + CW();

		::SelectObject(hDC, hMonoFont);

		sprintf(buf, "%d", BS - i);
		::DrawText(hDC, buf, strlen(buf), &rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

		rect.left = BOARD_LEFT + CW() * i;
		rect.right = rect.left + CW();
		rect.top = BOARD_TOP + CW() * BS;
		rect.bottom = BH(BS) - 4;
		//::SelectObject(hDC, hNumFont);

		buf[0] = 'A' + i;
		buf[1] = 0;
		::DrawText(hDC, buf, 1, &rect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
	}



	// 到了这里, 已经画了棋盘底纹, 标题和坐标, 接下来该画各交叉点及棋子了.
	// 在画之前先把座子摆好.

	// 摆座子
	int n;
	for(n = 0; n < game.moveCount && game.moves[n].addition; n++)
	{
		Move & m = game.moves[n];
		board[m.x][m.y] = m.color;
	}

	// 第一个非座子是实际棋谱的起点, 但我们不一定从起点开始做动画, 见下面的说明
	int firstMoveIndex = n;

	int num = 0;
	vector<int> num_to_index;

	// 手数映射, 因为中间可能有pass和多余棋子, 所以可能不是线性映射.
	num_to_index.push_back(-1);

	// 把firstNum之前的着法都视为座子, 但是手数要增加.
	// 有了这个处理, 我们就可以从任意手数开始做动画. 无需从头开始.
	for(n=firstMoveIndex; n<game.moveCount && num+1 < firstNum; n++)
	{
		Move & m = game.moves[n];
		m.addition = false; // 把对局过程中的多余棋子都当作实际的落子. 有时候是有这种加棋子摆变化的棋谱, 但我们没必要支持.
		bool put = PutStone(game.gameType, BS, board, m);
		if(put && !m.addition) // 只有落子成功才计算手数, 落到棋盘外等情况都认为是PASS
		{
			num++;
			num_to_index.push_back(n);
		}
	}

	firstMoveIndex = n; // firstNum 对应的位置, 数值可能和firstNum不同, 因为有座子和PASS

	// 摆好棋子了, 挨个格子画, 有棋子画棋子, 没棋子画交叉线.
	for(int i=0; i< BS; i++)
	{
		for(int j=0; j< BS; j++)
		{
			DrawCell(hDC, BS, board[i][j], i, j, BOARD_LEFT + CW()*i, BOARD_TOP + CW() * j, 0);
		}
	}

	// 现在初始画完了, 包括棋盘底纹, 网格线, 标题, 坐标和初始局面


	vector<CxImage *> images; // 各帧的图片

	CxImage * image = new CxImage(); // 第一帧
	images.push_back(image);
	image->CreateFromHBITMAP(hBmpBoard); // 第一帧是底图
	hBmpBoard = NULL; // 以后都是增量绘制, 不再需要底图. TODO: 这里是否需要删掉位图句柄?
	image->SetFrameDelay(100); // 第一帧固定延迟100. TODO: 是否应该可以设置?
	image = NULL;

	// 开始画其他动画帧了, 画到lastNum手为止.
	for(int i=firstMoveIndex; i<game.moveCount && num < lastNum; i++)
	{
		// 算法是很简单的, 对比这一帧前后的棋盘, 把变了的格子画到图片里, 贴上去
		memcpy(preBoard, board, sizeof(board)); // 先记住这一帧前的棋盘

		// 落子
		Move & m = game.moves[i];
		m.addition = false;
		bool put = PutStone(game.gameType, BS, board, m);
		if(put && !m.addition)
		{
			num++;
			num_to_index.push_back(i);
		}

		// 检测棋盘变化, 注意可能有提子, 所以变化的点可能是多个.
		// 我们把各个点的变化分开, 每个变化是一个小贴图.
		// 这样的好处是容易处理, 生成图片尺寸也较小.
		// 缺点也就是一直被诟病的, 提多子会打乱节奏.
		// 也许以后可以加一个选项, 支持多个变化点合并...

		vector<Cross> changes;
		for(int i=0; i<BS; i++)
		{
			for(int j=0; j<BS; j++)
			{
				if(preBoard[i][j] != board[i][j])
				{
					Cross pt = {i, j};
					if(preBoard[i][j] == None)
					{
						// 落子放在最前面, 显示的效果是先落子后提子
						changes.insert(changes.begin(), pt);
					}
					else
					{
						changes.push_back(pt);
					}
				}
			}
		}

		bool first = true;
		for(int n=0; n<changes.size(); n++)
		{
			int i = changes[n].x;
			int j = changes[n].y;

			CxImage * prevImage = images.back();

			RECT rect= { 0, 0, CW(), CW() };

			if(first && g_options.numbers > 0 &&  num - g_options.numbers > 0)
			{
				// 第一个变化是落子, 需要更新手数显示, 这里先把过期的手数删掉
				int removeNumber = num - g_options.numbers;
				int removeIndex = num_to_index[removeNumber];
				Move & rm = game.moves[removeIndex];

				// 把hBmpCell选为当前位图, 在上面画消掉手数的格子
				hOldBmp = (HBITMAP)::SelectObject(hDC, hBmpCell);
				DrawBoardBG(hDC, rect);
				DrawCell(hDC, BS, board[rm.x][rm.y], rm.x, rm.y, 0, 0, 0);
				::SelectObject(hDC, hOldBmp);

				// 作为动画贴上去
				image = new CxImage();
				image->CreateFromHBITMAP(hBmpCell);
				image->SetOffset(BOARD_LEFT + rm.x * CW(), BOARD_TOP + rm.y * CW());
				image->SetFrameDelay(0); // 理论上延迟是0, 但实际总是有一个很小的延迟
				images.push_back(image);

			}

			// 把hBmpCell选为当前位图, 在上面画变化的格子
			hOldBmp = (HBITMAP)::SelectObject(hDC, hBmpCell);
			DrawBoardBG(hDC, rect);
			::SelectObject(hDC, hNumFont);
			DrawCell(hDC, BS, board[i][j], i, j, 0, 0, m.addition || g_options.numbers <= 0 ? 0 : num);
			::SelectObject(hDC, hOldBmp);

			// 作为动画贴上去
			image = new CxImage();
			image->CreateFromHBITMAP(hBmpCell);
			image->SetOffset(BOARD_LEFT + i * CW(), BOARD_TOP + j * CW());
			image->SetFrameDelay(0); // 理论上延迟是0, 但实际总是有一个很小的延迟
			images.push_back(image);

			if(first)
			{
				if(num == firstNum)
				{
					prevImage->SetFrameDelay(150); // 第一手延迟固定150(为啥前面是100?) TODO: 是否应该可以设置?
				}
				else
				{
					prevImage->SetFrameDelay(g_options.delay); // 其他的延迟按用户设置的走
				}

				first = false;
			}
		}
	}

	// 最后一帧延迟长一点, 400, 然后再从第一帧开始播放
	images.back()->SetFrameDelay(400);

	// 颜色量化, 如果不用不量化, 那就是默认调色板+抖动, 效果很差.
	// TODO: 我们是按所有图片来量化, 这样的话有些元素会量化多次, 例如数字0.
	//       不知道这样做有没有问题, 是否应该另画一张典型图例来量化?
	CQuantizer q(256, 8);
    for(int i=0; i<images.size(); i++)
	{
		q.ProcessImage(images[i]->GetDIB());
	}

	// 按量化的结果生成调色版
    RGBQUAD* ppal=(RGBQUAD*)calloc(256 *sizeof(RGBQUAD),1);
    q.SetColorTable(ppal);


	for(int i=0; i<images.size(); i++)
	{
		// 在各帧上应用调色板
		images[i]->DecreaseBpp(8, true, ppal, 256);
	}

	// 保存成GIF文件
	CxIOFile cxIOFile;
	cxIOFile.Open(dstPath.c_str(), _T("wb"));
	CxImageGIF multiimage;

	multiimage.SetStdPalette();
	multiimage.SetLoops(0);
	multiimage.Encode(&cxIOFile, &images[0], images.size());

	for(int i=0; i<images.size(); i++)
	{
		delete images[i];
	}
	images.clear();

	if (ppal) free(ppal);
	::SelectObject(hDC, hOldBmp);
	::SelectObject(hDC, hOldFont);
	::SelectObject(hDC, hOldBrush);
	::DeleteObject(hNumFont);
	::DeleteObject(hTitleFont);
	::DeleteObject(hMonoFont);
	::DeleteObject(hBmpCell);
	::DeleteObject(hBmpBoard); // TODO: 之前不是把hBmpBoard设为NULL了吗?
	::DeleteObject(hBgBrush);
	::DeleteDC(hDC);
	return true;
}

// 按照配置, 把棋谱转成一个或多个GIF
bool Convert(const string & srcPath)
{
	Game game;
	if(!LoadSimpleSGF(game, srcPath))
	{
		MessageBox(NULL, srcPath.c_str(), "Parse SGF Failed", MB_OK|MB_ICONEXCLAMATION);
		return false;
	}

	bool actualSplit = g_options.RealSplit();
	int finalNum = game.GetRealMoveCount();
	if(actualSplit)
	{
		bool ret = true;
		int firstNum = 1;
		if(g_options.splitPeriodically)
		{
			while(firstNum <= finalNum)
			{
				int lastNum = firstNum + g_options.splitPoints[0] - 1;
				char buf[32];
				sprintf(buf, "%03d",firstNum);
				string sFirst = buf;
				sprintf(buf, "%03d",lastNum+2 < finalNum ? lastNum+2 :finalNum);
				string sLast = buf;
				const string dstPath = RemoveExtension(srcPath) + "(" + sFirst + "-" + sLast + ").gif";
				if(!Convert(game, dstPath, firstNum, lastNum + 2))
				{
					ret = false;
				}
				firstNum = lastNum +1;
			}
		}
		else
		{
			for(int i=0; i<=g_options.splitCount && firstNum <= finalNum; i++)
			{
				int lastNum = i<g_options.splitCount? g_options.splitPoints[i] - 1 : 1999;
				char buf[32];
				sprintf(buf, "%03d",firstNum);
				string sFirst = buf;
				sprintf(buf, "%03d",lastNum+2 < finalNum ? lastNum+2 :finalNum);
				string sLast = buf;
				const string dstPath = RemoveExtension(srcPath) + "(" + sFirst + "-" + sLast + ").gif";
				if(!Convert(game, dstPath, firstNum, lastNum+2))
				{
					ret = false;
				}
				firstNum = lastNum +1;
			}
		}
		return ret;
	}
	else
	{
		const string dstPath = RemoveExtension(srcPath) + ".gif";
		return Convert(game, dstPath, 1, finalNum);
	}
}


// 配置对话框, 用的是上个世纪的古老技术
HWND hWndEditDelay    = NULL;
HWND hWndEditNumbers  = NULL;
HWND hWndEditSplit    = NULL;
HWND hWndEditCW    = NULL;

string GetWindowString(HWND hWnd)
{
	char buf[2048];
	GetWindowText(hWnd, buf, 2048);
	return buf;
}

static BOOL CALLBACK DialogFunc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			::SendMessage(hWnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)::LoadIcon(::GetModuleHandle(NULL), MAKEINTRESOURCE(100)));
			::SendMessage(hWnd, WM_SETICON, (WPARAM)ICON_SMALL,
				(LPARAM)(HICON)::LoadImage(GetModuleHandle(NULL),
				MAKEINTRESOURCE(100),
				IMAGE_ICON,
				16,
				16,
				LR_DEFAULTCOLOR));

			hWndEditDelay = ::GetDlgItem(hWnd, IDC_EDIT_DELAY);
			hWndEditNumbers = ::GetDlgItem(hWnd, IDC_EDIT_NUMBERS);
			hWndEditSplit = ::GetDlgItem(hWnd, IDC_EDIT_SPLIT);
			hWndEditCW = ::GetDlgItem(hWnd, IDC_EDIT_CW);

			::SetWindowText(hWndEditDelay, g_options.GetDelayString().c_str());
			::SetWindowText(hWndEditNumbers, g_options.GetNumbersString().c_str());
			::SetWindowText(hWndEditSplit, g_options.GetSplitString().c_str());
			::SetWindowText(hWndEditCW, g_options.GetCWString().c_str());

			return 1;
		}
		break;
	case WM_COMMAND:
		if(wParam == IDOK)
		{
			g_options.SetDelayString(GetWindowString(hWndEditDelay));
			g_options.SetNumbersString(GetWindowString(hWndEditNumbers));
			g_options.SetSplitString(GetWindowString(hWndEditSplit));
			g_options.SetCWString(GetWindowString(hWndEditCW));
			EndDialog(hWnd, IDOK);
		}
		break;
	case WM_CLOSE:
		EndDialog(hWnd, IDCANCEL);
		return 1;
	}
	return 0;
}

void LoadSaveOptions(bool load)
{
	char buf[MAX_PATH];
	::GetModuleFileName(NULL, buf, MAX_PATH);
	string path = RemoveExtension(buf) + ".opt";
	if(load)
	{
		memset(&g_options, 0, sizeof(g_options));
		g_options.delay = 50;
		g_options.numbers = 1;
		g_options.splitCount = 0;
		g_options.cw = 23;
		FILE * file = fopen(path.c_str(), "rb");
		if(file)
		{
			fread(&g_options, sizeof(g_options), 1, file);
			fclose(file);
		}
	}
	else
	{
		FILE * file = fopen(path.c_str(), "wb+");
		if(file)
		{
			fwrite(&g_options, sizeof(g_options), 1, file);
			fclose(file);
		}
	}

}
bool ShowConfigDialog()
{
	InitCommonControls();
	LoadSaveOptions(true);
	INT_PTR ret = ::DialogBox(::GetModuleHandle(NULL), MAKEINTRESOURCE(200), NULL, (DLGPROC) DialogFunc);
	if(ret == IDOK)
	{
		LoadSaveOptions(false);
		return true;
	}
	else
	{
		return false;
	}
}


int _main(int argc, char * argv[])
{
	if(argc > 1)
	{
		if(!ShowConfigDialog())
		{
			printf("warning: failed to show options dialog.\n");
		}
		int ret = 0;
		for(int i=1; i<argc; i++)
		{
			if(!Convert(argv[i]))
			{
				ret++;
			}
		}
		return ret;
	}
	else
	{
		OPENFILENAME ofn;
		TCHAR szFile[MAX_PATH];

		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = szFile;
		ofn.lpstrFile[0] = '\0';
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = "Smart Go Format\0*.sgf\0\0";
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = NULL;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY ;

		if(GetOpenFileName(&ofn)==TRUE)
		{
			if(!ShowConfigDialog())
			{
				return -5;
			}
			int ret = Convert(ofn.lpstrFile) ? 0 : -2;
			return ret;
		}
		else
		{
			return -1;
		}
	}
}

int main(int argc, char * argv[])
{
	srand(::GetTickCount());
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	int ret = _main(argc, argv);
	GdiplusShutdown(gdiplusToken);
	return ret;
}
