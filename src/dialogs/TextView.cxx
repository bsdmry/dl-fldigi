// ----------------------------------------------------------------------------
//
//	TextView.cxx
//
// Copyright (C) 2006
//		Dave Freese, W1HKJ
//
// This file is part of fldigi.
//
// fldigi is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// fldigi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fldigi; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
// ----------------------------------------------------------------------------

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>

#include "TextView.h"
#include "main.h"

#include "macros.h"
#include "main.h"

#include "cw.h"
#include "misc.h"

#include <FL/Enumerations.H>
#include "File_Selector.h"

using namespace std;

//=====================================================================
// class textview
// a virtual base class for building either a text viewer or editor
// you cannot instantiate this class by itself
// only as a base for child classes
//
// text is stored in <string> arrays
// the attribute of each character is mirrored in another <string> array
//=====================================================================

textview :: textview( int x, int y, int w, int h, const char *label )
  : Fl_Widget( x, y, w, h, label )
{
	scrollbar = new Fl_Scrollbar( x+w-20, y+2, 20, h-4 );
	scrollbar->linesize( 1 );
	scrollbar->callback( _scrollbarCB, this );

	box( FL_DOWN_BOX );
	color( FL_WHITE );

	TextFont = FL_SCREEN;
	TextSize = FL_NORMAL_SIZE;
	
	for (int i = 0; i < 16; i++) {
		TextColor[i] = FL_BLACK;
	}
	TextColor[2] = FL_BLUE;
	TextColor[3] = FL_GREEN;
	TextColor[4] = FL_RED;
	
	wrappos = 0;
	cursorON = false;
	startidx = 0;
	laststartpos = lastendpos = string::npos;
		
	clear();
}

textview :: ~textview()
{
	delete scrollbar;
}

void textview::Show() {
	scrollbar->show();
	show();
}

void textview::Hide() {
	scrollbar->hide();
	hide();
}

int textview::handle(int event)
{
	return 0;
}

void textview::setFont(Fl_Font fnt)
{
	Fl::lock();
	TextFont = fnt;
	redraw();
	Fl::unlock();
	Fl::awake();
}

void textview::setFontSize(int siz)
{
	Fl::lock();
	TextSize = siz;
	redraw();
	Fl::unlock();
	Fl::awake();
}

void textview::setFontColor(Fl_Color clr)
{
}

int textview::lineCount()
{
	int cnt = 1;
	size_t len = buff.length();
	if (len == 0)
		cnt = 0;
	else
		for (size_t n = 0; n < len; n++)
			if (buff[n] == '\n') cnt++;
	return cnt;
}

size_t textview::linePosition(int linenbr)
{
	size_t len = buff.length();
	size_t pos = 0;
	while (linenbr && (pos < len) ) {
		if (buff[pos] == '\n')
			--linenbr;
		pos++;
	}
	if (pos == len) return 0;
	return pos;
}

void textview::draw_cursor()
{
	if (cursorStyle == NONE) return;
  int X = cursorX, Y = cursorY;
  
  typedef struct {
    int x1, y1, x2, y2;
  }
  Segment;

  Segment segs[ 5 ];
  int left, right, cursorWidth, midY;
  int nSegs = 0;
  int bot = Y  - charheight + 1;

  /* For cursors other than the block, make them around 2/3 of a character
     width, rounded to an even number of pixels so that X will draw an
     odd number centered on the stem at x. */
  cursorWidth = 4;
  X += cursorWidth/2;
  left = X - cursorWidth/2;
  right = left + cursorWidth;

  if (cursorON == false) {
	fl_color(FL_WHITE);
	fl_rectf ( X, Y - charheight, cursorWidth, charheight );
	return;
  }

  /* Create segments and draw cursor */
  if ( cursorStyle == CARET_CURSOR ) {
    midY = bot - charheight / 5;
    segs[ 0 ].x1 = left; segs[ 0 ].y1 = bot; segs[ 0 ].x2 = X; segs[ 0 ].y2 = midY;
    segs[ 1 ].x1 = X; segs[ 1 ].y1 = midY; segs[ 1 ].x2 = right; segs[ 1 ].y2 = bot;
    segs[ 2 ].x1 = left; segs[ 2 ].y1 = bot; segs[ 2 ].x2 = X; segs[ 2 ].y2 = midY - 1;
    segs[ 3 ].x1 = X; segs[ 3 ].y1 = midY - 1; segs[ 3 ].x2 = right; segs[ 3 ].y2 = bot;
    nSegs = 4;
  } else if ( cursorStyle == NORMAL_CURSOR ) {
    segs[ 0 ].x1 = left; segs[ 0 ].y1 = Y; segs[ 0 ].x2 = right; segs[ 0 ].y2 = Y;
    segs[ 1 ].x1 = X; segs[ 1 ].y1 = Y; segs[ 1 ].x2 = X; segs[ 1 ].y2 = bot;
    segs[ 2 ].x1 = left; segs[ 2 ].y1 = bot; segs[ 2 ].x2 = right; segs[ 2 ].y2 = bot;
    nSegs = 3;
  } else if ( cursorStyle == HEAVY_CURSOR ) {
    segs[ 0 ].x1 = X - 1; segs[ 0 ].y1 = Y; segs[ 0 ].x2 = X - 1; segs[ 0 ].y2 = bot;
    segs[ 1 ].x1 = X; segs[ 1 ].y1 = Y; segs[ 1 ].x2 = X; segs[ 1 ].y2 = bot;
    segs[ 2 ].x1 = X + 1; segs[ 2 ].y1 = Y; segs[ 2 ].x2 = X + 1; segs[ 2 ].y2 = bot;
    segs[ 3 ].x1 = left; segs[ 3 ].y1 = Y; segs[ 3 ].x2 = right; segs[ 3 ].y2 = Y;
    segs[ 4 ].x1 = left; segs[ 4 ].y1 = bot; segs[ 4 ].x2 = right; segs[ 4 ].y2 = bot;
    nSegs = 5;
  } else if ( cursorStyle == DIM_CURSOR ) {
    midY = Y + charheight / 2;
    segs[ 0 ].x1 = X; segs[ 0 ].y1 = Y; segs[ 0 ].x2 = X; segs[ 0 ].y2 = Y;
    segs[ 1 ].x1 = X; segs[ 1 ].y1 = midY; segs[ 1 ].x2 = X; segs[ 1 ].y2 = midY;
    segs[ 2 ].x1 = X; segs[ 2 ].y1 = bot; segs[ 2 ].x2 = X; segs[ 2 ].y2 = bot;
    nSegs = 3;
  } else if ( cursorStyle == BLOCK_CURSOR ) {
    right = X + maxcharwidth;
    segs[ 0 ].x1 = X; segs[ 0 ].y1 = Y; segs[ 0 ].x2 = right; segs[ 0 ].y2 = Y;
    segs[ 1 ].x1 = right; segs[ 1 ].y1 = Y; segs[ 1 ].x2 = right; segs[ 1 ].y2 = bot;
    segs[ 2 ].x1 = right; segs[ 2 ].y1 = bot; segs[ 2 ].x2 = X; segs[ 2 ].y2 = bot;
    segs[ 3 ].x1 = X; segs[ 3 ].y1 = bot; segs[ 3 ].x2 = X; segs[ 3 ].y2 = Y;
    nSegs = 4;
  }
  fl_color( FL_BLACK );

  for ( int k = 0; k < nSegs; k++ ) {
    fl_line( segs[ k ].x1, segs[ k ].y1, segs[ k ].x2, segs[ k ].y2 );
  }
}

string textview::findtext()
{
	size_t idx, wordstart, wordend;
	idx = startidx;
	int xc = 0, yc = 0;
	char c;
	size_t len = buff.length();
	static string selword;


	fl_font(TextFont, TextSize);
	selword = "";
	if (!len) return selword;
	while(idx < len) {
		if (yc > h()-4) 
			break;
		while (idx < len ) {
			c = buff[idx];
			if (c == '\n') {
				xc = 0;
				yc += fl_height();
				break;
			}
			xc += (int)(fl_width(c) + 0.5);
			idx++;
			if (	(yc < popy && yc > popy - charheight) &&
					(xc >= popx) ) {
				wordstart = buff.find_last_of(" \n", idx);
				if (wordstart == string::npos) wordstart = 0;
				wordend = buff.find_first_of(" ,\n", idx);
				if (wordend == string::npos) wordend = len;
				if (wordstart && wordend)
					selword = buff.substr(wordstart+1, wordend - wordstart - 1);
				else if (!wordstart && wordend)
					selword = buff.substr(0, wordend);
				return selword;
			}
		}
		idx++;
	}
	return selword;
}

void textview::drawall()
{
	int line = 0;
	size_t idx = 0;
	size_t startpos = string::npos;
	unsigned int xpos = 0;
	unsigned int ypos = 0;
	unsigned int len = 0;
	char c = 0;
	char cstr[] = " ";
	unsigned int	H = h() - 4, 
					W = w() - 4 - 20, 
					X = x() + 2, 
					Y = y() + 2;
  
	fl_font(TextFont, TextSize);
	charheight = fl_height();
	maxcharwidth = (int)fl_width('X');

	draw_box();
// resize the scrollbar to be a constant width
	scrollbar->resize( x()+w()-20, y()+2, 20, h()-4 );
	scrollbar->redraw();

	if ((len = buff.length()) == 0) {
		fl_push_clip( X, Y, W, H);
		cursorX = X; cursorY = Y + charheight;
		draw_cursor();
		fl_pop_clip();
    	return;
	}

	nlines = lineCount();
	line = nlines - H / charheight - scrollbar->value();
	  
	xpos = 0;
	ypos = charheight;
	startpos = idx = linePosition(line);
	
	fl_push_clip( X, Y, W, H );

	while(idx < len) {
		if (ypos > H) 
			break;
		while (idx < len ) { //&& (c = buff[idx]) != '\n') {
			c = buff[idx];
			if (c == '\n') {
				xpos = 0;
				ypos += charheight;
				idx++;
				break;
			}
			cstr[0] = c;
			fl_color (TextColor[(int)attr[idx]]);
			fl_draw ( cstr, 1, X + xpos, Y + ypos );
			xpos += (int)(fl_width(c) + 0.5);
			idx++;
		}
	}
	lastendpos = idx;
	laststartpos = startpos;
	
	cursorX = X + xpos; cursorY = Y + ypos;
	draw_cursor();
	fl_pop_clip();
}

void textview::drawchars()
{
	int line = 0;
	size_t idx = 0;
	size_t startpos = string::npos;
	unsigned int xpos = 0;
	unsigned int ypos = 0;
	unsigned int len = 0;
	char c = 0;
	char cstr[] = " ";
	unsigned int	H = h() - 4, 
					W = w() - 4 - 20, 
					X = x() + 2, 
					Y = y() + 2;
  
	if ((len = buff.length()) == 0) {
		drawall();
		return;
	}

	fl_font(TextFont, TextSize);
	charheight = fl_height();
	maxcharwidth = (int)fl_width('X');

	nlines = lineCount();
	line = nlines - H / charheight - scrollbar->value();
	  
	xpos = 0;
	ypos = charheight;
	startpos = idx = linePosition(line);
	
	if (startpos != string::npos && startpos == laststartpos) {
		if (lastendpos != string::npos)
			while (idx < lastendpos ) {
				if (ypos > H)
					break;
				while (idx < lastendpos) {
					c = buff[idx];
					if (c == '\n') {
						xpos = 0;
						ypos += charheight;
						idx++;
						break;
					}
					xpos += (int)(fl_width(c) + 0.5);
					idx++;
				}
			}
	} else {
		draw_box();
		scrollbar->resize( x()+w()-20, y()+2, 20, h()-4 );
		scrollbar->redraw();
	}
	
	fl_push_clip( X, Y, W, H );

	while(idx < len) {
		if (ypos > H) 
			break;
		while (idx < len ) { //&& (c = buff[idx]) != '\n') {
			c = buff[idx];
			if (c == '\n') {
				xpos = 0;
				ypos += charheight;
				idx++;
				break;
			}
			cstr[0] = c;
			fl_color (TextColor[(int)attr[idx]]);
			fl_draw ( cstr, 1, X + xpos, Y + ypos );
			xpos += (int)(fl_width(c) + 0.5);
			idx++;
		}
	}
	lastendpos = idx;
	laststartpos = startpos;
	
	cursorX = X + xpos; cursorY = Y + ypos;
	draw_cursor();
	fl_pop_clip();
}

void textview::drawbs()
{
}

void textview::draw()
{
	if (damage() & FL_DAMAGE_ALL) {
		drawall();
		return;
	}
	if (damage() & (FL_DAMAGE_ALL | 1)) {
		drawchars();
		return;
	}
	if (damage() & (FL_DAMAGE_ALL | 2)) {
		drawbs();
		return;
	}
		
}

void textview::scrollbarCB()
{
	damage(FL_DAMAGE_ALL);
}

void textview::_backspace()
{
	int c;
	size_t lastcrlf = buff.rfind('\n');

	if (lastcrlf == string::npos) lastcrlf = 0;
	
	if (attr[attr.length() - 1] == -1) { // soft linefeed skip over
		buff.erase(buff.length()-1);
		attr.erase(attr.length()-1);
		wrappos = 0;
		lastcrlf = buff.rfind('\n');
		if (lastcrlf == string::npos) lastcrlf = 0;
		fl_font(TextFont, TextSize);
		for (size_t i = lastcrlf; i < buff.length(); i++) {
			wrappos += (int)(fl_width(buff[i]));
		}
	}

	c = buff[buff.length()-1];
	if (c == '\n') {
		buff.erase(buff.length()-1);
		attr.erase(attr.length()-1);
		wrappos = 0;
		lastcrlf = buff.rfind('\n');
		if (lastcrlf == string::npos) lastcrlf = 0;
		fl_font(TextFont, TextSize);
		for (size_t i = lastcrlf; i < buff.length(); i++) {
			wrappos += (int)(fl_width(buff[i]));
		}
	} else {
		buff.erase(buff.length()-1);
		attr.erase(attr.length()-1);
		fl_font(TextFont, TextSize);
		wrappos -= (int)(fl_width(c) + 0.5);
	}
	damage(FL_DAMAGE_ALL);
}

void textview::add( char c, int attribute)
{
    Fl::lock();
	if (c == 0x08) {
		if (buff.length() > 0)
			_backspace();
        Fl::unlock();
        Fl::awake();
		return;
	} else {
		if (c >= ' ' && c <= '~') {
			buff += c;
			attr += attribute;
			fl_font(TextFont, TextSize);
			charwidth = (int)(fl_width(c) + 0.5);
			if (charwidth > maxcharwidth)
				maxcharwidth = charwidth;
			wrappos += charwidth;
		} else if (c == '\n') {
			buff += c;
			attr += attribute;
			wrappos = 0;
		}
	}

	int endpos = w() - 24 - maxcharwidth;
	if (wrappos >= endpos) {
		size_t lastspace = buff.find_last_of(' ');
		if (!wordwrap 
			|| lastspace == string::npos 
			|| (buff.length() - lastspace) >= 10
			|| (buff.length() - lastspace) == 1) {
			buff += '\n';
			attr += -1; // soft linefeed attribute
			wrappos = 0;
			damage(1);
		} else {
			buff.insert(lastspace+1, 1, '\n');
			attr.insert(lastspace+1, 1, -1);
			wrappos = 0;
			fl_font(TextFont, TextSize);
			for (size_t i = lastspace+2; i < buff.length(); i++)
				wrappos += (int)(fl_width(buff[i]) + 0.5);
			damage(FL_DAMAGE_ALL);
		}
	} else
		damage(1);
    Fl::unlock();
    Fl::awake();
	setScrollbar();
}

void textview::add( char *text, int attr )
{
	for (unsigned int i = 0; i < strlen(text); i++)
		add(text[i], attr);
	return;
}

void textview::clear()
{
	Fl::lock();
	buff.erase();
	attr.erase();
	wrappos = 0;
	startidx = 0;
	laststartpos = lastendpos = string::npos;
	setScrollbar();
	damage(FL_DAMAGE_ALL);
	Fl::unlock();
	Fl::awake();
}


void textview :: setScrollbar()
{
	int lines = lineCount();
	double size;

	fl_font(TextFont, TextSize);
	charheight = fl_height();
	scrollbar->range (lines, 0);
	if (lines * charheight <= h()-4)
		size = 1.0;
	else
		size = (double)(h()-4) / (double)(lines * charheight);
	if (size < 0.08) size = 0.08;
	scrollbar->slider_size( size );
}

void textview :: rebuildsoft(int W)
{
	size_t lfpos, chpos;
	if (W == w())
		return; // same width no rebuild needed
// remove all soft linefeeds
	while ((lfpos = attr.find(-1)) != string::npos) {
		buff.erase(lfpos, 1);
		attr.erase(lfpos, 1);
	}
	int endpos = W - 24 - maxcharwidth;
	wrappos = 0;
	chpos = 0;
	while (chpos < buff.length()) {
		if (buff[chpos] == '\n')
			wrappos = 0;
		else
			fl_font(TextFont, TextSize);
			wrappos += (int)(fl_width(buff[chpos]) + 0.5);
		if (wrappos >= endpos) {
			size_t lastspace = buff.find_last_of(' ', chpos);
			if (!wordwrap 
				|| lastspace == string::npos 
				|| (chpos - lastspace) >= 10
				|| (chpos == lastspace) ) {
				buff.insert(chpos, 1, '\n');
				attr.insert(chpos, 1, -1);
				wrappos = 0;
				chpos++;
			} else {
				buff.insert(lastspace+1, 1, '\n');
				attr.insert(lastspace+1, 1, -1);
				wrappos = 0;
				chpos++;
				fl_font(TextFont, TextSize);
				for (size_t i = lastspace+2; i < chpos; i++)
					wrappos += (int)(fl_width(buff[i]) + 0.5);
			}
		}
		chpos++;
	}
}

void textview :: resize( int x, int y, int w, int h )
{
	rebuildsoft(w);
	Fl_Widget::resize( x, y, w, h );
	setScrollbar();
}


//=====================================================================
// Class TextView
// Viewer for received text
// derived from Class textview
//
// redefines the handle() and menu_cb() functions specified in the
// base class.  All other functions are in the base class
//=====================================================================

void TextView::saveFile()
{
	char * fn = File_Select(
					"Select ASCII text file", 
					"*.txt",
					"", 0);
	if (fn) {
		ofstream out(fn);
		out << buff;
		out.close();
	}
}

Fl_Menu_Item TextView::viewmenu[] = {
	{"@-9$returnarrow &QRZ this call",	0, 0 }, 				// 0
	{"@-9-> &Call",				0, 0 },							// 1
	{"@-9-> &Name",				0, 0 },							// 2
	{"@-9-> QT&H",				0, 0 },							// 3
	{"@-9-> &Locator",			0, 0 },							// 4
	{"@-9-> &RSTin",			0, 0, 0, FL_MENU_DIVIDER },		// 5
	{"Insert divider",			0, 0 },							// 6
	{"C&lear",				0, 0 },								// 7
//	{"&Copy",				0, 0, 0, FL_MENU_DIVIDER },
	{"Save to &file...",			0, 0, 0, FL_MENU_DIVIDER },	// 8
	{"Word &wrap",				0, 0, 0, FL_MENU_TOGGLE	 },		// 9
	{ 0 }
};

int viewmenuNbr = 10;

TextView::TextView( int x, int y, int w, int h, const char *label )
	: textview ( x, y, w, h, label )
{
	cursorStyle = NONE;// BLOCK_CURSOR;
	cursorON = true;
	wordwrap = true;
	viewmenu[9].set();
}

void TextView::menu_cb(int val)
{
//	handle(FL_UNFOCUS);
	switch (val) {
	case 0: // select call & do qrz query
		menu_cb(1);
		extern void QRZquery();
		QRZquery();
		break;
	case 1: // select call
		inpCall->value(findtext().c_str());
		break;
	case 2: // select name
		inpName->value(findtext().c_str());
		break;
	case 3: // select QTH
		inpQth->value(findtext().c_str());
		break;
	case 4: // select locator
		inpLoc->value(findtext().c_str());
		break;
	case 5: // select RST rcvd
		inpRstIn->value(findtext().c_str());
		break;
	case 6:
		add("\n	    <<================>>\n", RCV);
		break;
	case 7:
		clear();
		break;
//	case 8: // clipboard copy
//		clipboard_copy();
//		break;
	case 8:
		saveFile();
		break;

	case 9: // wordwrap toggle
		wordwrap = !wordwrap;
		if (wordwrap)
			viewmenu[9].set();
		else
			viewmenu[9].clear();
		break;
	}
}

int TextView::handle(int event)
{
// handle events inside the textview and invoked by Right Mouse button or scrollbar
	if (Fl::event_inside( this )) {
		const Fl_Menu_Item * m;
		int xpos = Fl::event_x();
		int ypos = Fl::event_y();
		if (xpos > x() + w() - 20) {
			scrollbar->handle(event);
			return 1;
		}
		if (event == FL_PUSH && Fl::event_button() == 3) {
			popx = xpos - x();
			popy = ypos - y();
			m = viewmenu->popup(xpos, ypos, 0, 0, 0);
			if (m) {
				for (int i = 0; i < viewmenuNbr; i++)
					if (m == &viewmenu[i]) {
						menu_cb(i);
						break;
					}
			}
			return 1;
		}
	}
	return 0;
}

//=====================================================================
// Class TextEdit
// derived from base class textview
// redfines the handle() and menu_cb() functions specified in the
// base class.  All other functions are in the base class
//=====================================================================

Fl_Menu_Item editmenu[] = {
	{"clear",	0, 0, 0, FL_MENU_DIVIDER },
	{"File",	0, 0, 0, FL_MENU_DIVIDER },
	{"^t",	0, 0 },
	{"^r",	0, 0, 0, FL_MENU_DIVIDER },
	{"Picture", 0, 0 },
	{0}
};
int editmenuNbr = 5;

TextEdit::TextEdit( int x, int y, int w, int h, const char *label )
	: textview ( x, y, w, h, label )
{
	chrptr = 0;
	bkspaces = 0;
	textview::cursorStyle = HEAVY_CURSOR;
	PauseBreak = false;
	wordwrap = false;
}


void TextEdit::readFile()
{
	char * fn = File_Select(
					"Select ASCII text file", 
					"*.txt",
					"", 0);
	if (fn)  {
		string inbuff;
		ifstream in(fn);
		if (in) {
			char ch;
			while (!in.eof()) {
				ch = in.get();
				inbuff += ch;
			}
			in.close();
			add (inbuff.c_str());
		}
	}
}

void TextEdit::menu_cb(int val)
{
	if (val == 0) {
		clear();
		chrptr = 0;
		bkspaces = 0;
	}
	if (val == 1)
		readFile();
	if (val == 2 && buff.empty()) {
		fl_lock(&trx_mutex);
		trx_state = STATE_TX;
		fl_unlock(&trx_mutex);
		wf->set_XmtRcvBtn(true);
	}
	if (val == 3)
		add("^r");
	if (val == 4)
		if (active_modem->get_mode() == MODE_MFSK16)
			active_modem->makeTxViewer(0,0);
}

void TextEdit::clear() {
	textview::clear();
	chrptr = 0;
	PauseBreak = false;
}

int TextEdit::handle_fnckey(int key) {
	int b = key - FL_F - 1;
	if (b > 9)
		return 0;
	
	b += (altMacros ? 10 : 0);
	if (!(macros.text[b]).empty())
		macros.execute(b);

	return 1;
}

int TextEdit::handle_key() {
	int key = Fl::event_key();
	
	if (key == FL_Escape) {
		clear();
		active_modem->set_stopflag(true);
		return 1;
	}
	
	if (key == FL_Pause) {
		if (trx_state != STATE_TX) {
			fl_lock(&trx_mutex);
			trx_state = STATE_TX;
			fl_unlock(&trx_mutex);
			wf->set_XmtRcvBtn(true);
		} else
			PauseBreak = true;
		return 1;
	}
	
	if (key == (FL_KP + '+')) {
		if (active_modem == cw_modem) active_modem->incWPM();
		return 1;
	}
	if (key == (FL_KP + '-')) {
		if (active_modem == cw_modem) active_modem->decWPM();
		return 1;
	}
	if (key == (FL_KP + '*')) {
		if (active_modem == cw_modem) active_modem->toggleWPM();
		return 1;
	}

	if (key >= FL_F && key <= FL_F_Last)
		return handle_fnckey(key);
		
	if (key == FL_Tab && active_modem == cw_modem) {
		while (chrptr < buff.length()) {
			attr[chrptr] = 2;
			chrptr++;
		}
		redraw();
		return 1;
	}

	if (key == FL_Left) {
		active_modem->searchDown();
		return 1;
	}
	if (key == FL_Right) {
		active_modem->searchUp();
		return 1;
	}
		
	if (key == FL_Enter) {
		add('\n');
		return 1;
	}
	
	if (key == FL_BackSpace) {
		add (0x08);
		if (chrptr > buff.length()) {
			chrptr = buff.length();
			bkspaces++;
		}
		return 1;
	}
	
	const char *ch = Fl::event_text();
	add(ch);
	return 1;
}

int TextEdit::handle(int event)
{
// handle events inside the textedit widget
	if (event == FL_UNFOCUS && Fl::focus() != this) {
		textview::cursorON = false;
		redraw();
		return 1;
	}
	if (event == FL_FOCUS && Fl::focus() == this) {
		textview::cursorON = true;
		redraw();
		return 1;
	}
	if (event == FL_KEYBOARD) {
		return handle_key();
	}
	if (Fl::event_inside( this )) {
		const Fl_Menu_Item * m;
		int xpos = Fl::event_x();
		int ypos = Fl::event_y();
		if (xpos > x() + w() - 20) {
			scrollbar->handle(event);
			return 1;
		}
		if (event == FL_PUSH && Fl::event_button() == 3) {
			popx = xpos - x();
			popy = ypos - y();
			m = editmenu->popup(xpos, ypos, 0, 0, 0);
			if (m) {
				for (int i = 0; i < editmenuNbr; i++)
					if (m == &editmenu[i]) {
						menu_cb(i);
						break;
					}
			}
			return 1;
		}

		switch (event) {
			case FL_PUSH:
				textview::cursorON = true;
				redraw();
				Fl::focus(this);
				return 1;
			case FL_FOCUS:
				textview::cursorON = true;
				redraw();
				return 1;
			case FL_UNFOCUS:
				textview::cursorON = false;
				redraw();
				return 1;
		}
	}
	return 0;
}

int TextEdit::nextChar()
{
	if (bkspaces) {
		--bkspaces;
		return 0x08;
	}
	if (PauseBreak) {
		PauseBreak = false;
		return 0x03;
	}
	if (buff.empty()) return 0;
	if (chrptr == buff.length()) return 0;
	if (attr[chrptr] == -1) {
		chrptr++;
		if (chrptr == buff.length()) return 0;
	}
	Fl::lock();
	attr[chrptr] = 4;
	redraw();
	Fl::unlock();
//	Fl::awake();
	return (buff[chrptr++]);
}

void TextEdit::cursorON() 
{ 
	textview::cursorON = true; 
	redraw();
}

