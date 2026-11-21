// Renders the MONOLITH editor offscreen to PNG (design review / CI artefact).
// Usage: MonolithUiSnapshot <out.png> [scale] [presetIndex] [notes...]
#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "monolith_ui.png");
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.0f;
    const int preset = argc > 3 ? juce::String (argv[3]).getIntValue() : 0;

    monolith::MonolithAudioProcessor proc;
    proc.setRateAndBufferSizeDetails (48000.0, 256);
    proc.prepareToPlay (48000.0, 256);
    proc.applyFactoryPreset (preset);

    // Play a chord for a second so meters/field telemetry are alive in the snapshot
    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer midi;
    for (int a = 4; a < argc; ++a)
        midi.addEvent (juce::MidiMessage::noteOn (1, juce::String (argv[a]).getIntValue(), 0.9f), 0);
    for (int b = 0; b < 200; ++b) { buf.clear(); proc.processBlock (buf, midi); midi.clear(); }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize ((int) (ed->getWidth() * scale), (int) (ed->getHeight() * scale));
    // let timers tick so telemetry-driven visuals settle
    for (int i = 0; i < 40; ++i)
    {
        buf.clear(); proc.processBlock (buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (17);
    }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::FileOutputStream os (out);
    os.setPosition (0); os.truncate();
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
    std::cout << "wrote " << out.getFullPathName() << " " << img.getWidth() << "x" << img.getHeight() << "\n";
    ed.reset();
    return 0;
}
