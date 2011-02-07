//
// Urho3D Engine
// Copyright (c) 2008-2011 Lasse ��rni
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "Precompiled.h"
#include "Input.h"
#include "LineEdit.h"
#include "Text.h"
#include "UIEvents.h"

#include "DebugNew.h"

LineEdit::LineEdit(const std::string& name, const std::string& text) :
    BorderImage(name),
    mLastFont(0),
    mLastFontSize(0),
    mCursorPosition(0),
    mCursorBlinkRate(1.0f),
    mCursorBlinkTimer(0.0f),
    mMaxLength(0),
    mEchoCharacter(0),
    mDefocusable(true),
    mDefocus(false)
{
    mClipChildren = true;
    mEnabled = true;
    mFocusable = true;
    mText = new Text();
    mCursor = new BorderImage();
    addChild(mText);
    addChild(mCursor);
    
    // Show cursor on top of text
    mCursor->setPriority(1);
    setText(text);
}

LineEdit::~LineEdit()
{
}

void LineEdit::setStyle(const XMLElement& element, ResourceCache* cache)
{
    BorderImage::setStyle(element, cache);
    
    if (element.hasChildElement("cursorblinkrate"))
        setCursorBlinkRate(element.getChildElement("cursorblinkrate").getFloat("value"));
    if (element.hasChildElement("echocharacter"))
    {
        std::string text = element.getChildElement("echocharacter").getString("value");
        if (text.length())
            setEchoCharacter(text[0]);
    }
    if (element.hasChildElement("maxlength"))
        setMaxLength(element.getChildElement("maxlength").getInt("value"));
    if (element.hasChildElement("cursorposition"))
        setCursorPosition(element.getChildElement("cursorposition").getInt("value"));
    
    XMLElement textElem = element.getChildElement("text");
    if (textElem)
    {
        if (textElem.hasAttribute("value"))
            setText(textElem.getString("value"));
        mText->setStyle(textElem, cache);
    }
    XMLElement cursorElem = element.getChildElement("cursor");
    if (cursorElem)
        mCursor->setStyle(cursorElem, cache);
}

void LineEdit::update(float timeStep)
{
    if (mCursorBlinkRate > 0.0f)
        mCursorBlinkTimer = fmodf(mCursorBlinkTimer + mCursorBlinkRate * timeStep, 1.0f);
    else
        mCursorBlinkTimer = 0.0f;
    
    // Update cursor position if font has changed
    if ((mText->getFont() != mLastFont) || (mText->getFontSize() != mLastFontSize))
    {
        mLastFont = mText->getFont();
        mLastFontSize = mText->getFontSize();
        updateCursor();
    }
   
    bool cursorVisible = false;
    if (mFocus)
        cursorVisible = mCursorBlinkTimer < 0.5f;
    mCursor->setVisible(cursorVisible);
    
    if (mDefocus)
    {
        setFocus(false);
        mDefocus = false;
    }
}

void LineEdit::onClick(const IntVector2& position, const IntVector2& screenPosition, int buttons, int qualifiers)
{
    if (buttons & MOUSEB_LEFT)
    {
        IntVector2 textPosition = mText->screenToElement(screenPosition);
        const std::vector<IntVector2>& charPositions = mText->getCharPositions();
        for (unsigned i = charPositions.size() - 1; i < charPositions.size(); --i)
        {
            if (textPosition.mX >= charPositions[i].mX)
            {
                setCursorPosition(i);
                break;
            }
        }
    }
}

void LineEdit::onKey(int key, int buttons, int qualifiers)
{
    bool changed = false;
    bool cursorMoved = false;
    
    switch (key)
    {
    case KEY_LEFT:
        if (mCursorPosition > 0)
        {
            --mCursorPosition;
            cursorMoved = true;
        }
        break;
        
    case KEY_RIGHT:
        if (mCursorPosition < mLine.length())
        {
            ++mCursorPosition;
            cursorMoved = true;
        }
        break;
        
    case KEY_DELETE:
        if (mCursorPosition < mLine.length())
        {
            if (mCursorPosition)
                mLine = mLine.substr(0, mCursorPosition) + mLine.substr(mCursorPosition + 1);
            else
                mLine = mLine.substr(mCursorPosition + 1);
            changed = true;
        }
        break;
    }
    
    if (changed)
    {
        updateText();
        updateCursor();
        
        using namespace TextChanged;
        
        VariantMap eventData;
        eventData[P_ELEMENT] = (void*)this;
        eventData[P_TEXT] = mLine;
        sendEvent(EVENT_TEXTCHANGED, eventData);
    }
    else if (cursorMoved)
        updateCursor();
}

void LineEdit::onChar(unsigned char c, int buttons, int qualifiers)
{
    bool changed = false;
    
    if (c == '\b')
    {
        if ((mLine.length()) && (mCursorPosition))
        {
            if (mCursorPosition < mLine.length())
                mLine = mLine.substr(0, mCursorPosition - 1) + mLine.substr(mCursorPosition);
            else
                mLine = mLine.substr(0, mCursorPosition - 1);
            --mCursorPosition;
            changed = true;
        }
    }
    else if (c == '\r')
    {
        using namespace TextFinished;
        
        VariantMap eventData;
        eventData[P_ELEMENT] = (void*)this;
        sendEvent(EVENT_TEXTFINISHED, eventData);
    }
    else if (c == 27)
    {
        if (mDefocusable)
            mDefocus = true;
    }
    else if ((c >= 0x20) && ((!mMaxLength) || (mLine.length() < mMaxLength)))
    {
        static std::string charStr;
        charStr.resize(1);
        charStr[0] = c;
        if (mCursorPosition == mLine.length())
            mLine += charStr;
        else
            mLine = mLine.substr(0, mCursorPosition) + charStr + mLine.substr(mCursorPosition);
        ++mCursorPosition;
        changed = true;
    }
    
    if (changed)
    {
        updateText();
        updateCursor();
        
        using namespace TextChanged;
        
        VariantMap eventData;
        eventData[P_ELEMENT] = (void*)this;
        eventData[P_TEXT] = mLine;
        sendEvent(EVENT_TEXTCHANGED, eventData);
    }
}

void LineEdit::setText(const std::string& text)
{
    mLine = text;
    mText->setText(text);
    updateText();
}

void LineEdit::setCursorPosition(unsigned position)
{
    if (position > mLine.length())
        position = mLine.length();
    
    if (position != mCursorPosition)
    {
        mCursorPosition = position;
        updateCursor();
    }
}

void LineEdit::setCursorBlinkRate(float rate)
{
    mCursorBlinkRate = max(rate, 0.0f);
}

void LineEdit::setMaxLength(unsigned length)
{
    mMaxLength = length;
}

void LineEdit::setEchoCharacter(char c)
{
    mEchoCharacter = c;
    updateText();
}

void LineEdit::setDefocusable(bool enable)
{
    mDefocusable = enable;
}

void LineEdit::updateText()
{
    if (!mEchoCharacter)
        mText->setText(mLine);
    else
    {
        std::string echoText;
        echoText.resize(mLine.length());
        for (unsigned i = 0; i < mLine.length(); ++i)
            echoText[i] = mEchoCharacter;
        mText->setText(echoText);
    }
    if (mCursorPosition > mLine.length())
    {
        mCursorPosition = mLine.length();
        updateCursor();
    }
}

void LineEdit::updateCursor()
{
    int x = 0;
    const std::vector<IntVector2>& charPositions = mText->getCharPositions();
    if (charPositions.size())
        x = mCursorPosition < charPositions.size() ? charPositions[mCursorPosition].mX : charPositions.back().mX;
    
    mCursor->setPosition(mText->getPosition() + IntVector2(x, 0));
    mCursor->setSize(mCursor->getWidth(), mText->getRowHeight());
    
    // Scroll if necessary
    int sx = -getChildOffset().mX;
    int left = mClipBorder.mLeft;
    int right = getWidth() - mClipBorder.mLeft - mClipBorder.mRight - mCursor->getWidth();
    if (x - sx > right)
        sx = x - right;
    if (x - sx < left)
        sx = x - left;
    setChildOffset(IntVector2(-sx, 0));
    
    // Restart blinking
    mCursorBlinkTimer = 0.0f;
}
