#pragma once
#include "AppStyle.h"
#include <charconv>
// A numeric field: click to type, drag its displayed number vertically to scrub.
// Text is committed atomically; incomplete/invalid text never changes a cue.
class DragNumber final : public juce::Component, public juce::SettableTooltipClient {
public:
    DragNumber() {
        setWantsKeyboardFocus(true);addChildComponent(input);
        input.setJustification(juce::Justification::centred);input.setMultiLine(false);
        input.onReturnKey=[this]{commit();};input.onEscapeKey=[this]{cancel();};
        input.onFocusLost=[this]{commit();};
        input.onTextChange=[this]{input.setColour(juce::TextEditor::outlineColourId,ui::line);};
        setTooltip("Click to type. Drag up/down to adjust; hold Shift for finer control. Enter to confirm, Esc to cancel.");
    }
    void setRange(int low,int high){minimum=low;maximum=high;setValue(value,juce::dontSendNotification);}
    void setValue(int next,juce::NotificationType notification=juce::sendNotificationSync) {
        next=juce::jlimit(minimum,maximum,next);const bool different=value!=next;value=next;
        if(!input.isVisible())repaint();
        if(different && notification!=juce::dontSendNotification && onValueChange)onValueChange();
    }
    int getValue() const{return value;}
    void setDigits(int count){digits=count;}
    void setNumberFont(float size){fontSize=size;input.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),size,juce::Font::plain)));}
    bool commit() {
        if(!input.isVisible())return true;
        const auto s=input.getText().trim().toStdString();int next=0;
        const auto p=std::from_chars(s.data(),s.data()+s.size(),next);
        if(s.empty() || p.ec!=std::errc{} || p.ptr!=s.data()+s.size()) {
            input.setColour(juce::TextEditor::outlineColourId,juce::Colour(0xffda8673));return false;
        }
        input.setVisible(false);setValue(next);repaint();return true;
    }
    void cancel(){input.setVisible(false);repaint();}
    void beginEdit() {
        if(!isEnabled())return;
        if(input.isVisible()){input.grabKeyboardFocus();return;}
        if(onEditStart)onEditStart();
        input.setText(juce::String(value),false);input.setVisible(true);input.grabKeyboardFocus();input.selectAll();
    }
    void paint(juce::Graphics& g) override {
        auto r=getLocalBounds().toFloat().reduced(.5f);
        g.setColour(ui::field.withMultipliedAlpha(isEnabled()?1.f:.45f));g.fillRoundedRectangle(r,3.f);
        g.setColour(hasKeyboardFocus(false)?ui::accent:ui::line);g.drawRoundedRectangle(r,3.f,1.f);
        g.setColour(ui::text.withMultipliedAlpha(isEnabled()?1.f:.35f));
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),fontSize,juce::Font::plain)));
        g.drawText(juce::String(value).paddedLeft('0',digits),getLocalBounds(),juce::Justification::centred,false);
    }
    void resized() override {input.setBounds(getLocalBounds());}
    void mouseDown(const juce::MouseEvent&) override {
        if(!isEnabled())return;
        grabKeyboardFocus();dragStart=value;dragging=false;
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if(!isEnabled() || std::abs(e.getDistanceFromDragStartY())<3)return;
        if(!dragging){dragging=true;if(onEditStart)onEditStart();}
        const int pixels=e.mods.isShiftDown()?24:6;
        setValue(dragStart-e.getDistanceFromDragStartY()/pixels);
    }
    void mouseUp(const juce::MouseEvent&) override {if(!dragging && isEnabled())beginEdit();}
    bool keyPressed(const juce::KeyPress& key) override {
        if(key==juce::KeyPress::returnKey){beginEdit();return true;}
        if(key==juce::KeyPress::upKey || key==juce::KeyPress::downKey){
            if(onEditStart)onEditStart();
            setValue(value+(key==juce::KeyPress::upKey?1:-1));return true;
        }
        return false;
    }
    std::function<void()> onValueChange,onEditStart;
private:
    friend class SceneEditor; // Regression tests exercise the production text editor.
    int minimum=0,maximum=59,value=0,dragStart=0,digits=1;
    float fontSize=16.f;
    bool dragging=false;
    juce::TextEditor input;
};
