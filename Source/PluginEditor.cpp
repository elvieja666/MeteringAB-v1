#include "PluginEditor.h"
#include "Text.h"

namespace Colours
{
    const juce::Colour background { 0xff16181d };
    const juce::Colour panel      { 0xff20232b };
    const juce::Colour outline    { 0xff2e323c };
    const juce::Colour text       { 0xffe8e6e3 };
    const juce::Colour dim        { 0xff8a8f98 };
    const juce::Colour accent     { 0xfff2b544 };
    const juce::Colour good       { 0xff6fcf97 };
    const juce::Colour bad        { 0xffeb5757 };
}

// ---------------------------------------------------------------------------
//  PreciseSlider
// ---------------------------------------------------------------------------
PreciseSlider::PreciseSlider()
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 22);
    setTextBoxIsEditable (true);                 // <- clic en el número para escribir
    setWantsKeyboardFocus (true);
}

void PreciseSlider::mouseDown (const juce::MouseEvent& e)
{
    // Con Shift, hace falta arrastrar 10 veces más: ajuste fino.
    setMouseDragSensitivity (e.mods.isShiftDown() ? 2500 : 250);
    juce::Slider::mouseDown (e);
}

// ---------------------------------------------------------------------------
//  Look & feel
// ---------------------------------------------------------------------------
MeteringABLookAndFeel::MeteringABLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colours::background);
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxBackgroundColourId, Colours::background);
    setColour (juce::Slider::textBoxOutlineColourId, Colours::outline);
    setColour (juce::Slider::textBoxHighlightColourId, Colours::accent.withAlpha (0.4f));
    setColour (juce::TextEditor::backgroundColourId, Colours::background);
    setColour (juce::TextEditor::textColourId, Colours::text);
    setColour (juce::TextEditor::focusedOutlineColourId, Colours::accent);
    setColour (juce::CaretComponent::caretColourId, Colours::accent);
    setColour (juce::ComboBox::backgroundColourId, Colours::panel);
    setColour (juce::ComboBox::outlineColourId, Colours::outline);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::arrowColourId, Colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, Colours::background);
    setColour (juce::TextButton::buttonColourId, Colours::panel);
    setColour (juce::TextButton::buttonOnColourId, Colours::accent);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, Colours::background);
    setColour (juce::ToggleButton::textColourId, Colours::text);
    setColour (juce::ToggleButton::tickColourId, Colours::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, Colours::dim);
    setColour (juce::Label::textColourId, Colours::text);
}

void MeteringABLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle  = startAngle + pos * (endAngle - startAngle);
    const float track = 4.0f;

    juce::Path back;
    back.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::outline);
    g.strokePath (back, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, angle, true);
    g.setColour (Colours::accent);
    g.strokePath (value, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto knobRadius = radius - track * 3.0f;
    g.setColour (Colours::panel);
    g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2, knobRadius * 2);

    const auto tip = centre.getPointOnCircumference (knobRadius - 4.0f, angle);
    g.setColour (Colours::text);
    g.drawLine ({ centre.getPointOnCircumference (knobRadius * 0.35f, angle), tip }, 2.5f);
}

// ---------------------------------------------------------------------------
//  Editor
// ---------------------------------------------------------------------------
MeteringABEditor::MeteringABEditor (MeteringABProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);
    auto& apvts = processor.apvts;

    auto addCombo = [&] (juce::ComboBox& box, const char* paramId, std::unique_ptr<ComboAttachment>& att)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramId)))
            box.addItemList (choice->choices, 1);

        addAndMakeVisible (box);
        att = std::make_unique<ComboAttachment> (apvts, paramId, box);
    };

    addCombo (modeBox, ParamIDs::mode, modeAtt);
    addCombo (matchBox, ParamIDs::match, matchAtt);
    addCombo (measureBox, ParamIDs::measure, measureAtt);

    for (auto* l : { &matchLabel, &measureLabel })
    {
        l->setColour (juce::Label::textColourId, Colours::dim);
        l->setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (*l);
    }
    matchLabel.setText ("Matcheo", juce::dontSendNotification);
    measureLabel.setText ("Medición"_u, juce::dontSendNotification);

    // Link ID: flechitas + número editable.
    linkSlider.setSliderStyle (juce::Slider::IncDecButtons);
    linkSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 26);
    linkSlider.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
    linkLabel.setText ("Link ID", juce::dontSendNotification);
    linkLabel.setColour (juce::Label::textColourId, Colours::dim);
    linkLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (linkSlider);
    addAndMakeVisible (linkLabel);
    linkAtt = std::make_unique<SliderAttachment> (apvts, ParamIDs::link, linkSlider);

    setupKnob (manualSlider,  manualLabel,  "Ganancia manual");
    setupKnob (windowSlider,  windowLabel,  "Ventana");
    setupKnob (limitSlider,   limitLabel,   "Límite auto"_u);
    setupKnob (latencySlider, latencyLabel, "Latencia (muestras)");

    manualAtt  = std::make_unique<SliderAttachment> (apvts, ParamIDs::manual,  manualSlider);
    windowAtt  = std::make_unique<SliderAttachment> (apvts, ParamIDs::window,  windowSlider);
    limitAtt   = std::make_unique<SliderAttachment> (apvts, ParamIDs::limit,   limitSlider);
    latencyAtt = std::make_unique<SliderAttachment> (apvts, ParamIDs::latency, latencySlider);

    // Doble clic vuelve al valor por defecto.
    manualSlider.setDoubleClickReturnValue (true, 0.0);
    windowSlider.setDoubleClickReturnValue (true, 3.0);
    limitSlider.setDoubleClickReturnValue (true, 18.0);
    latencySlider.setDoubleClickReturnValue (true, 0.0);

    const juce::StringArray listenNames { "Procesado", "Original", "Delta" };

    for (int i = 0; i < 3; ++i)
    {
        listenButtons[i].setButtonText (listenNames[i]);
        listenButtons[i].setClickingTogglesState (false);
        listenButtons[i].onClick = [this, i] { setListen (i); };
        addAndMakeVisible (listenButtons[i]);
    }

    listenButtons[0].setConnectedEdges (juce::Button::ConnectedOnRight);
    listenButtons[1].setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    listenButtons[2].setConnectedEdges (juce::Button::ConnectedOnLeft);

    addAndMakeVisible (holdButton);
    holdAtt = std::make_unique<ButtonAttachment> (apvts, ParamIDs::hold, holdButton);

    statusLabel.setFont (juce::FontOptions (13.0f));
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    shownPre  = processor.displayPre.load();
    shownPost = processor.displayPost.load();
    shownGain = processor.displayGain.load();

    setSize (640, 450);
    timerCallback();
    startTimerHz (20);
}

MeteringABEditor::~MeteringABEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MeteringABEditor::setupKnob (PreciseSlider& s, juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, Colours::dim);
    label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (s);
    addAndMakeVisible (label);
}

void MeteringABEditor::setListen (int index)
{
    if (auto* param = processor.apvts.getParameter (ParamIDs::listen))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
        param->endChangeGesture();
    }
}

void MeteringABEditor::timerCallback()
{
    // Movimiento suave de los números en pantalla.
    auto ease = [] (float& shown, float target) { shown += (target - shown) * 0.35f; };
    ease (shownPre,  processor.displayPre.load());
    ease (shownPost, processor.displayPost.load());
    ease (shownGain, processor.displayGain.load());

    const bool sendMode = (int) processor.apvts.getRawParameterValue (ParamIDs::mode)->load() == 0;

    if (sendMode != isSendMode)
    {
        isSendMode = sendMode;

        for (juce::Component* c : std::initializer_list<juce::Component*> {
                 &matchBox, &measureBox, &matchLabel, &measureLabel, &manualSlider, &windowSlider,
                 &limitSlider, &latencySlider, &manualLabel, &windowLabel, &limitLabel, &latencyLabel,
                 &listenButtons[0], &listenButtons[1], &listenButtons[2], &holdButton })
            c->setVisible (! isSendMode);
    }

    const int listen = (int) processor.apvts.getRawParameterValue (ParamIDs::listen)->load();
    for (int i = 0; i < 3; ++i)
        listenButtons[i].setToggleState (i == listen, juce::dontSendNotification);

    const bool manual = (int) processor.apvts.getRawParameterValue (ParamIDs::match)->load() == 1;
    manualSlider.setEnabled (manual);
    manualSlider.setAlpha (manual ? 1.0f : 0.4f);

    // Mensaje de estado.
    const int link = (int) processor.apvts.getRawParameterValue (ParamIDs::link)->load();

    if (processor.duplicateSend.load())
    {
        statusLabel.setText ("Hay más de un Send con el Link ID "_u + juce::String (link) + ": cambiá uno."_u, juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, Colours::bad);
    }
    else if (isSendMode)
    {
        statusLabel.setText ("Send activo en Link ID " + juce::String (link) + ". Poné un Metering A/B en Receive al final de la cadena."_u, juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, Colours::good);
    }
    else if (processor.sendConnected.load())
    {
        statusLabel.setText ("Conectado al Send del Link ID " + juce::String (link) + ".", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, Colours::good);
    }
    else
    {
        statusLabel.setText ("No hay ningún Send sonando en el Link ID "_u + juce::String (link) + " (dale play).", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, Colours::bad);
    }

    repaint (0, 90, getWidth(), 100);
}

void MeteringABEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    // Título
    g.setColour (Colours::accent);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("METERING A/B", 20, 14, 210, 32, juce::Justification::centredLeft);

    // Panel de medidores
    auto meterArea = juce::Rectangle<int> (20, 94, getWidth() - 40, 90);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (meterArea.toFloat(), 8.0f);

    const auto unit = (int) processor.apvts.getRawParameterValue (ParamIDs::measure)->load() == 1 ? " LUFS" : " dB";

    auto formatLevel = [unit] (float db)
    {
        return db <= -119.0f ? juce::String ("—"_u) : juce::String (db, 1) + unit;
    };

    struct Readout { juce::String title, value; juce::Colour colour; };
    std::vector<Readout> readouts;

    if (isSendMode)
    {
        readouts.push_back ({ "Nivel original", formatLevel (shownPre), Colours::text });
    }
    else
    {
        const auto gainText = (shownGain >= 0 ? "+" : "") + juce::String (shownGain, 2) + " dB";
        readouts.push_back ({ "Original (Send)", formatLevel (shownPre),  Colours::text });
        readouts.push_back ({ "Procesado",       formatLevel (shownPost), Colours::text });
        readouts.push_back ({ "Ganancia aplicada", gainText,              Colours::accent });
    }

    const int colWidth = meterArea.getWidth() / (int) readouts.size();

    for (size_t i = 0; i < readouts.size(); ++i)
    {
        auto col = meterArea.withX (meterArea.getX() + (int) i * colWidth).withWidth (colWidth).reduced (10, 12);
        g.setColour (Colours::dim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (readouts[i].title, col.removeFromTop (18), juce::Justification::centred);
        g.setColour (readouts[i].colour);
        g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
        g.drawText (readouts[i].value, col, juce::Justification::centred);
    }

    if (isSendMode)
    {
        g.setColour (Colours::dim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText ("Este Metering A/B va PRIMERO en la cadena de efectos.\n"
                          "Poné otro Metering A/B en modo Receive al FINAL, con el mismo Link ID.\n"_u
                          "Los controles de matcheo y el A/B están en el Receive."_u,
                          juce::Rectangle<int> (20, 210, getWidth() - 40, 80), juce::Justification::centred, 4);
    }
    else
    {
        g.setColour (Colours::dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("Clic en un número para escribir el valor exacto  ·  Shift + arrastrar = ajuste fino  ·  Doble clic = valor por defecto"_u,
                    20, getHeight() - 24, getWidth() - 40, 18, juce::Justification::centred);
    }
}

void MeteringABEditor::resized()
{
    auto area = getLocalBounds().reduced (20, 0);

    // Fila superior: título | modo | link ID
    auto top = area.removeFromTop (60).withTrimmedTop (14).withHeight (32);
    top.removeFromLeft (210);
    linkSlider.setBounds (top.removeFromRight (110));
    linkLabel.setBounds (top.removeFromRight (60));
    top.removeFromRight (12);
    modeBox.setBounds (top);

    statusLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (6);
    area.removeFromTop (90);   // medidores (se dibujan en paint)
    area.removeFromTop (14);

    // Fila de A/B y opciones
    auto row = area.removeFromTop (48);
    auto listenArea = row.removeFromLeft (240).withTrimmedTop (16).withHeight (30);
    const int bw = listenArea.getWidth() / 3;
    for (auto& b : listenButtons)
        b.setBounds (listenArea.removeFromLeft (bw));

    row.removeFromLeft (12);
    auto matchArea = row.removeFromLeft (100);
    matchLabel.setBounds (matchArea.removeFromTop (16));
    matchBox.setBounds (matchArea.withHeight (30));

    row.removeFromLeft (10);
    auto measureArea = row.removeFromLeft (120);
    measureLabel.setBounds (measureArea.removeFromTop (16));
    measureBox.setBounds (measureArea.withHeight (30));

    row.removeFromLeft (10);
    holdButton.setBounds (row.withTrimmedTop (16).withHeight (30));

    // Perillas
    area.removeFromTop (12);
    auto knobs = area.removeFromTop (150);
    const int kw = knobs.getWidth() / 4;

    auto placeKnob = [&] (PreciseSlider& s, juce::Label& l)
    {
        auto k = knobs.removeFromLeft (kw);
        l.setBounds (k.removeFromTop (18));
        s.setBounds (k.reduced (8, 0));
    };

    placeKnob (manualSlider,  manualLabel);
    placeKnob (windowSlider,  windowLabel);
    placeKnob (limitSlider,   limitLabel);
    placeKnob (latencySlider, latencyLabel);
}
