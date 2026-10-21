#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
namespace ui {
inline const juce::Colour background{0xff202123}, panel{0xff27282b}, field{0xff191a1c},
    line{0xff414347}, text{0xffe2e3e5}, muted{0xffa2a5aa}, accent{0xff7da6b5};
class LookAndFeel final : public juce::LookAndFeel_V4 {
public:
    LookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId,background);
        setColour(juce::Label::textColourId,text);
        setColour(juce::TextButton::buttonColourId,panel);
        setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff785b32));
        setColour(juce::TextButton::textColourOffId,text);
        setColour(juce::TextButton::textColourOnId,juce::Colours::white);
        setColour(juce::TextEditor::backgroundColourId,field);
        setColour(juce::TextEditor::textColourId,text);
        setColour(juce::TextEditor::outlineColourId,line);
        setColour(juce::TextEditor::focusedOutlineColourId,accent);
        setColour(juce::ComboBox::backgroundColourId,field);
        setColour(juce::ComboBox::outlineColourId,line);
        setColour(juce::ComboBox::textColourId,text);
        setColour(juce::PopupMenu::backgroundColourId,panel);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xff3c4b51));
        setColour(juce::ToggleButton::textColourId,text);
        setColour(juce::ToggleButton::tickColourId,accent);
        setColour(juce::TabbedComponent::outlineColourId,line);
        setColour(juce::TabbedButtonBar::tabTextColourId,muted);
        setColour(juce::TabbedButtonBar::frontTextColourId,text);
    }
    juce::Font getTextButtonFont(juce::TextButton&,int) override { return juce::Font(juce::FontOptions(14.f)); }
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& c,bool over,bool down) override {
        auto r=b.getLocalBounds().toFloat().reduced(.5f);
        g.setColour(c.brighter(down?.12f:over?.06f:0.f).withMultipliedAlpha(b.isEnabled()?1.f:.4f));g.fillRoundedRectangle(r,3.f);
        g.setColour(b.hasKeyboardFocus(true)?accent:line);g.drawRoundedRectangle(r,3.f,1.f);
    }
};
}
