#include "MambaLookAndFeel.h"

using namespace juce;

MambaLookAndFeel::MambaLookAndFeel()
    : mainFont_ (Font (FontOptions (14.f))),
      smallFont_(Font (FontOptions (11.f)))
{
    // Base colours fed into V4 colour scheme so inherited widgets look right.
    setColour(ResizableWindow::backgroundColourId,     MambaColors::background);
    setColour(DocumentWindow::backgroundColourId,      MambaColors::background);

    setColour(Slider::backgroundColourId,              MambaColors::border);
    setColour(Slider::trackColourId,                   MambaColors::aiBlue);
    setColour(Slider::thumbColourId,                   MambaColors::aiBlue);
    setColour(Slider::rotarySliderFillColourId,        MambaColors::aiBlue);
    setColour(Slider::rotarySliderOutlineColourId,     MambaColors::border);
    setColour(Slider::textBoxTextColourId,             MambaColors::textPrimary);
    setColour(Slider::textBoxBackgroundColourId,       MambaColors::surface);
    setColour(Slider::textBoxOutlineColourId,          MambaColors::border);
    setColour(Slider::textBoxHighlightColourId,        MambaColors::aiBlueHalf);

    setColour(TextButton::buttonColourId,              MambaColors::surface);
    setColour(TextButton::buttonOnColourId,            MambaColors::aiBlueHalf);
    setColour(TextButton::textColourOffId,             MambaColors::textPrimary);
    setColour(TextButton::textColourOnId,              MambaColors::aiBlue);

    setColour(ComboBox::backgroundColourId,            MambaColors::surface);
    setColour(ComboBox::outlineColourId,               MambaColors::border);
    setColour(ComboBox::textColourId,                  MambaColors::textPrimary);
    setColour(ComboBox::arrowColourId,                 MambaColors::textSecondary);
    setColour(ComboBox::focusedOutlineColourId,        MambaColors::aiBlue);

    setColour(PopupMenu::backgroundColourId,           MambaColors::surface);
    setColour(PopupMenu::textColourId,                 MambaColors::textPrimary);
    setColour(PopupMenu::highlightedBackgroundColourId,MambaColors::aiBlueHalf);
    setColour(PopupMenu::highlightedTextColourId,      MambaColors::textPrimary);

    setColour(Label::textColourId,                     MambaColors::textSecondary);
    setColour(Label::backgroundColourId,               Colours::transparentBlack);
    setColour(Label::outlineColourId,                  Colours::transparentBlack);
    setColour(Label::textWhenEditingColourId,           MambaColors::textPrimary);
}

// =============================================================================
// Rotary slider
// =============================================================================

void MambaLookAndFeel::drawRotarySlider(Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPosProportional,
                                         float rotaryStartAngle,
                                         float rotaryEndAngle,
                                         Slider& slider)
{
    const float cx = x + width  * 0.5f;
    const float cy = y + height * 0.5f;

    // Leave room for the position dot outside the body.
    const float outerRadius = std::min(width, height) * 0.5f - 2.f;
    const float trackWidth  = 3.f;
    const float bodyRadius  = outerRadius - trackWidth - 2.f;

    const float angle = rotaryStartAngle +
                        sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // --- Track ring (background arc) ---
    {
        Path track;
        track.addCentredArc(cx, cy, outerRadius, outerRadius,
                            0.f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(MambaColors::border);
        g.strokePath(track, PathStrokeType(trackWidth, PathStrokeType::curved,
                                           PathStrokeType::rounded));
    }

    // --- Ghost arc: AI target position (aiBlueHalf) ---
    {
        const float aiVal = static_cast<float>(
            static_cast<double>(slider.getProperties().getWithDefault("aiValue", -1.0)));
        if (aiVal >= 0.f) {
            const float aiAngle = rotaryStartAngle +
                                  aiVal * (rotaryEndAngle - rotaryStartAngle);
            Path ghost;
            ghost.addCentredArc(cx, cy, outerRadius, outerRadius,
                                0.f, rotaryStartAngle, aiAngle, true);
            g.setColour(MambaColors::aiBlueHalf);
            g.strokePath(ghost, PathStrokeType(trackWidth + 1.f, PathStrokeType::curved,
                                               PathStrokeType::rounded));
        }
    }

    // --- Filled arc (AI value or manual) ---
    {
        const bool isManual = static_cast<bool>(slider.getProperties()["manualOverride"]);
        const Colour arcColour = isManual ? MambaColors::amber : MambaColors::aiBlue;

        Path filled;
        filled.addCentredArc(cx, cy, outerRadius, outerRadius,
                             0.f, rotaryStartAngle, angle, true);
        g.setColour(arcColour);
        g.strokePath(filled, PathStrokeType(trackWidth, PathStrokeType::curved,
                                            PathStrokeType::rounded));
    }

    // --- Knob body ---
    {
        const float bx = cx - bodyRadius;
        const float by = cy - bodyRadius;
        const float bd = bodyRadius * 2.f;

        // Shadow ring
        g.setColour(MambaColors::background.withAlpha(0.6f));
        g.fillEllipse(bx - 1.f, by - 1.f, bd + 2.f, bd + 2.f);

        // Main body fill
        g.setColour(MambaColors::surfaceHighlight);
        g.fillEllipse(bx, by, bd, bd);

        // Subtle inner highlight (top-left)
        ColourGradient highlight(MambaColors::border.withAlpha(0.4f), cx - bodyRadius * 0.4f,
                                 cy - bodyRadius * 0.4f,
                                 Colours::transparentBlack, cx + bodyRadius * 0.3f,
                                 cy + bodyRadius * 0.3f, true);
        g.setGradientFill(highlight);
        g.fillEllipse(bx, by, bd, bd);

        // Border ring
        g.setColour(MambaColors::border);
        g.drawEllipse(bx, by, bd, bd, 1.f);
    }

    // --- Position dot ---
    {
        const float dotDist   = bodyRadius * 0.72f;
        const float dotRadius = 2.5f;
        const float dotX = cx + dotDist * std::sin(angle) - dotRadius;
        const float dotY = cy - dotDist * std::cos(angle) - dotRadius;

        g.setColour(MambaColors::textPrimary);
        g.fillEllipse(dotX, dotY, dotRadius * 2.f, dotRadius * 2.f);
    }
}

// =============================================================================
// Linear slider
// =============================================================================

void MambaLookAndFeel::drawLinearSlider(Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPos, float /*minSliderPos*/,
                                         float /*maxSliderPos*/,
                                         Slider::SliderStyle style,
                                         Slider& slider)
{
    if (style == Slider::LinearVertical) {
        // Thin vertical track
        const float trackW = 3.f;
        const float trackX = x + (width - trackW) * 0.5f;
        const float trackTop    = static_cast<float>(y);
        const float trackBottom = static_cast<float>(y + height);

        g.setColour(MambaColors::border);
        g.fillRoundedRectangle(trackX, trackTop, trackW,
                               trackBottom - trackTop, trackW * 0.5f);

        g.setColour(MambaColors::aiBlue);
        g.fillRoundedRectangle(trackX, sliderPos, trackW,
                               trackBottom - sliderPos, trackW * 0.5f);

        // Thumb
        const float thumbH = 6.f;
        const float thumbW = 14.f;
        g.setColour(MambaColors::aiBlue);
        g.fillRoundedRectangle(trackX - (thumbW - trackW) * 0.5f,
                               sliderPos - thumbH * 0.5f,
                               thumbW, thumbH, thumbH * 0.5f);
        return;
    }

    // Horizontal (default)
    const float trackH = 3.f;
    const float trackY = y + (height - trackH) * 0.5f;
    const float trackLeft  = static_cast<float>(x);
    const float trackRight = static_cast<float>(x + width);

    // Track background
    g.setColour(MambaColors::border);
    g.fillRoundedRectangle(trackLeft, trackY, trackRight - trackLeft,
                           trackH, trackH * 0.5f);

    // Filled portion
    const bool isManual = slider.getProperties()["manualOverride"];
    g.setColour(isManual ? MambaColors::amber : MambaColors::aiBlue);
    g.fillRoundedRectangle(trackLeft, trackY, sliderPos - trackLeft,
                           trackH, trackH * 0.5f);

    // Ghost arc: AI target position (aiBlueHalf)
    {
        const float aiVal = static_cast<float>(
            static_cast<double>(slider.getProperties().getWithDefault("aiValue", -1.0)));
        if (aiVal >= 0.f && aiVal <= 1.f) {
            const float range = trackRight - trackLeft;
            const float aiPos = trackLeft + aiVal * range;
            g.setColour(MambaColors::aiBlueHalf);
            g.fillRect(static_cast<int>(aiPos) - 1, static_cast<int>(trackY) - 3, 2, static_cast<int>(trackH) + 6);
        }
    }

    // Thumb
    const float thumbW = 6.f;
    const float thumbH = 14.f;
    g.setColour(MambaColors::aiBlue);
    g.fillRoundedRectangle(sliderPos - thumbW * 0.5f,
                           trackY - (thumbH - trackH) * 0.5f,
                           thumbW, thumbH, thumbW * 0.5f);
}

// =============================================================================
// Button
// =============================================================================

void MambaLookAndFeel::drawButtonBackground(Graphics& g, Button& button,
                                             const juce::Colour& /*backgroundColour*/,
                                             bool isHighlighted,
                                             bool isDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const float corner = bounds.getHeight() * 0.5f; // pill shape

    const bool isOn = button.getToggleState();

    // Fill
    if (isDown)
        g.setColour(MambaColors::aiBlue.withAlpha(0.35f));
    else if (isOn)
        g.setColour(MambaColors::aiBlueHalf);
    else if (isHighlighted)
        g.setColour(MambaColors::surfaceHighlight);
    else
        g.setColour(MambaColors::surface);

    g.fillRoundedRectangle(bounds, corner);

    // Border
    const Colour borderCol = (isOn || isHighlighted)
                             ? MambaColors::aiBlue.withAlpha(0.7f)
                             : MambaColors::border;
    g.setColour(borderCol);
    g.drawRoundedRectangle(bounds, corner, 1.f);
}

void MambaLookAndFeel::drawButtonText(Graphics& g, TextButton& button,
                                       bool /*isHighlighted*/, bool /*isDown*/)
{
    const bool isOn = button.getToggleState();
    g.setFont(mainFont_);
    g.setColour(isOn ? MambaColors::aiBlue : MambaColors::textPrimary);
    g.drawFittedText(button.getButtonText(),
                     button.getLocalBounds().reduced(4, 2),
                     Justification::centred, 1);
}

// =============================================================================
// ComboBox
// =============================================================================

void MambaLookAndFeel::drawComboBox(Graphics& g, int width, int height,
                                     bool isButtonDown,
                                     int /*buttonX*/, int /*buttonY*/,
                                     int /*buttonW*/, int /*buttonH*/,
                                     ComboBox& box)
{
    const Rectangle<float> bounds(0.f, 0.f, static_cast<float>(width),
                                  static_cast<float>(height));
    const float corner = 4.f;

    g.setColour(isButtonDown ? MambaColors::surfaceHighlight : MambaColors::surface);
    g.fillRoundedRectangle(bounds, corner);

    const Colour outlineCol = box.hasKeyboardFocus(false)
                              ? MambaColors::aiBlue
                              : MambaColors::border;
    g.setColour(outlineCol);
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.f);

    // Arrow chevron
    const float arrowSize = 8.f;
    const float arrowX = width - arrowSize - 6.f;
    const float arrowY = (height - arrowSize * 0.5f) * 0.5f;
    Path arrow;
    arrow.startNewSubPath(arrowX, arrowY);
    arrow.lineTo(arrowX + arrowSize * 0.5f, arrowY + arrowSize * 0.5f);
    arrow.lineTo(arrowX + arrowSize, arrowY);
    g.setColour(MambaColors::textSecondary);
    g.strokePath(arrow, PathStrokeType(1.5f, PathStrokeType::curved,
                                       PathStrokeType::rounded));
}

Font MambaLookAndFeel::getComboBoxFont(ComboBox&)
{
    return mainFont_;
}

void MambaLookAndFeel::positionComboBoxText(ComboBox& box, Label& label)
{
    label.setBounds(4, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}

// =============================================================================
// Popup menu
// =============================================================================

void MambaLookAndFeel::drawPopupMenuBackground(Graphics& g, int width, int height)
{
    g.setColour(MambaColors::surface);
    g.fillRect(0, 0, width, height);
    g.setColour(MambaColors::border);
    g.drawRect(0, 0, width, height, 1);
}

void MambaLookAndFeel::drawPopupMenuItem(Graphics& g,
                                          const Rectangle<int>& area,
                                          bool isSeparator,
                                          bool /*isActive*/,
                                          bool isHighlighted,
                                          bool isTicked,
                                          bool /*hasSubMenu*/,
                                          const String& text,
                                          const String& /*shortcutKeyText*/,
                                          const Drawable* /*icon*/,
                                          const Colour* /*textColour*/)
{
    if (isSeparator) {
        g.setColour(MambaColors::border);
        g.fillRect(area.getX() + 4, area.getCentreY(), area.getWidth() - 8, 1);
        return;
    }

    if (isHighlighted) {
        g.setColour(MambaColors::aiBlueHalf);
        g.fillRect(area);
    }

    g.setFont(mainFont_);
    g.setColour(isHighlighted ? MambaColors::textPrimary : MambaColors::textSecondary);
    g.drawFittedText(text, area.reduced(8, 2), Justification::centredLeft, 1);

    if (isTicked) {
        g.setColour(MambaColors::aiBlue);
        const float d = area.getHeight() * 0.3f;
        g.fillEllipse(area.getRight() - d - 6.f,
                      area.getCentreY() - d * 0.5f, d, d);
    }
}

// =============================================================================
// Label
// =============================================================================

void MambaLookAndFeel::drawLabel(Graphics& g, Label& label)
{
    g.fillAll(label.findColour(Label::backgroundColourId));

    if (!label.isBeingEdited()) {
        const Colour textCol = label.findColour(Label::textColourId);
        g.setColour(textCol);
        g.setFont(getLabelFont(label));
        g.drawFittedText(label.getText(),
                         label.getLocalBounds().reduced(2, 0),
                         label.getJustificationType(), 2,
                         label.getMinimumHorizontalScale());
    }
}

Font MambaLookAndFeel::getLabelFont(Label& label)
{
    // If the label has a font set, use it; otherwise fall back by size.
    const auto& f = label.getFont();
    return (f.getHeight() <= 12.f) ? smallFont_ : mainFont_;
}
